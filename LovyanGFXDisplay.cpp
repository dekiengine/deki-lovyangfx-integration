#include <deki/Engine.h>  // ColorFormat, FrameBufferBytes
#include "LovyanGFXDisplay.h"

#ifdef ESP32
#include <algorithm>
#include <cstring>
#include <deki/LogSystem.h>
#include <deki/providers/Memory.h>
#endif

#ifdef ESP32
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_task_wdt.h>  // feeds the watchdog during long display writes
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <soc/soc_caps.h>

// Must come after esp_idf_version.h, which defines ESP_IDF_VERSION and
// ESP_IDF_VERSION_VAL. Tested earlier, the #if is 0 and esp_cache.h is left
// out without warning, so esp_cache_msync is undeclared.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <esp_cache.h>
#include <esp_dma_utils.h>  // esp_dma_malloc, which gives DMA and cache alignment
#endif

#include <LovyanGFX.hpp>
#endif

namespace DekiLovyanGfx
{

#ifdef ESP32

LovyanGFXDisplay::LovyanGFXDisplay()
    : tft(nullptr),
      m_DisplayWidth(320),
      m_DisplayHeight(240),
      initialized(false),
      buffers{ nullptr, nullptr },
      m_BufferPixelCount(0),
      m_RenderIndex(0),
      m_DmaInFlight(false),
      m_UsePSRAM(false),
      m_DoubleBuffer(false),
      m_SwapBytes(false),
      m_ActiveOverlay(nullptr)
{
}

LovyanGFXDisplay::~LovyanGFXDisplay()
{
    Shutdown();
}

static uint16_t* AllocateDisplayBuffer(size_t bufferBytes, bool usePSRAM, const char* label)
{
    // Through the engine, which decides placement. The panel driver reads
    // this buffer by DMA, so it must be DMA-capable memory, not just internal:
    // some internal regions on this chip are out of a peripheral's reach, and
    // DMA from one corrupts the display without any error.
    //
    // usePSRAM comes from the board's config and decides where the buffer
    // goes. DMA capability is needed either way, since the panel driver reads
    // the buffer by DMA wherever it lives.
    const Deki::Memory::Region region = usePSRAM ? Deki::Memory::External : Deki::Memory::Internal;
    uint16_t* buf = (uint16_t*)Deki::Memory::AllocateDma(bufferBytes, region);

    // `label` only names the buffer in the log lines below. The engine
    // identifies the allocation by its call site, which is the same for both
    // buffers.

    if (buf)
    {
        DEKI_LOG_INTERNAL("LovyanGFX: Allocated %s (%zu bytes, psram=%d)", label, bufferBytes, (int)usePSRAM);
    }
    else
    {
        DEKI_LOG_ERROR("LovyanGFX: no room for %s (%zu bytes) in %s RAM", label, bufferBytes,
                       usePSRAM ? "external" : "DMA-capable internal");
    }

    return buf;
}

bool LovyanGFXDisplay::InitializeWithDevice(lgfx::LGFX_Device* device, int32_t width, int32_t height, bool swapBytes,
                                            bool usePSRAM, bool doubleBuffer)
{
    if (initialized)
    {
        return true;
    }

    if (!device)
    {
        DEKI_LOG_ERROR("LovyanGFXDisplay::InitializeWithDevice: device is null");
        return false;
    }

    tft = device;
    m_DisplayWidth = width;
    m_DisplayHeight = height;
    m_UsePSRAM = usePSRAM;
    m_DoubleBuffer = doubleBuffer;
    m_SwapBytes = swapBytes;
    m_BufferPixelCount = width * height;

    if (usePSRAM || doubleBuffer)
    {
        size_t bufferBytes = m_BufferPixelCount * sizeof(uint16_t);

        buffers[0] = AllocateDisplayBuffer(bufferBytes, usePSRAM, "buffer[0]");
        if (!buffers[0])
        {
            DEKI_LOG_ERROR("LovyanGFX: Failed to allocate primary buffer (%zu bytes)", bufferBytes);
            return false;
        }
        memset(buffers[0], 0, bufferBytes);

        if (doubleBuffer)
        {
            buffers[1] = AllocateDisplayBuffer(bufferBytes, usePSRAM, "buffer[1]");
            if (!buffers[1])
            {
                DEKI_LOG_WARNING("LovyanGFX: Failed to allocate second buffer, falling back to single-buffer mode");
                m_DoubleBuffer = false;
            }
            else
            {
                memset(buffers[1], 0, bufferBytes);
            }
        }
    }
    // Otherwise passthrough mode: no display buffers; Present pushes the framebuffer directly.

    m_RenderIndex = 0;
    m_DmaInFlight = false;

    initialized = true;
    DEKI_LOG_INTERNAL("LovyanGFX display initialized %dx%d (psram=%d, doubleBuffer=%d)", width, height,
                      usePSRAM ? 1 : 0, m_DoubleBuffer ? 1 : 0);

    return true;
}

bool LovyanGFXDisplay::Initialize(int32_t width, int32_t height)
{
    // The device comes from LGFXDisplayPanel through InitializeWithDevice().
    DEKI_LOG_ERROR("LovyanGFXDisplay::Initialize() called directly - use LGFXDisplayPanel component instead");
    return false;
}

void LovyanGFXDisplay::Shutdown()
{
    if (!initialized)
    {
        return;
    }

    // The async task finishes its frame and stops before anything it uses goes.
    SetAsyncPresent(false);

    // Wait for any in-flight DMA before freeing buffers
    if (m_DmaInFlight && tft)
    {
        tft->waitDMA();
        m_DmaInFlight = false;
    }

    for (int i = 0; i < 2; i++)
    {
        if (buffers[i])
        {
            Deki::Memory::Free(buffers[i]);
            buffers[i] = nullptr;
        }
    }
    FreeBands();
    m_BufferPixelCount = 0;
    DEKI_LOG_INTERNAL("LovyanGFX: Freed display buffers");

    initialized = false;
}

bool LovyanGFXDisplay::SetAsyncPresent(bool on)
{
    if (!on)
    {
        if (m_PresentTask)
        {
            WaitForAsyncPresent();
            vTaskDelete(static_cast<TaskHandle_t>(m_PresentTask));
            m_PresentTask = nullptr;
        }
        if (m_FrameReady)
        {
            vQueueDelete(static_cast<QueueHandle_t>(m_FrameReady));
            m_FrameReady = nullptr;
        }
        if (m_FreeStaging)
        {
            vSemaphoreDelete(static_cast<SemaphoreHandle_t>(m_FreeStaging));
            m_FreeStaging = nullptr;
        }
        for (uint8_t*& staging : m_Staging)
        {
            heap_caps_free(staging);
            staging = nullptr;
        }
        m_StagingBytes = 0;
        m_NextStaging = 0;
        m_AsyncPresent = false;
        return true;
    }
    if (m_AsyncPresent)
    {
        return true;
    }

#if !defined(CONFIG_FREERTOS_UNICORE) && (SOC_CPU_CORES_NUM > 1)
    // Two frames in external memory: the task sends one while the engine
    // copies the next frame into the other, then draws on into its own
    // buffer. With one, the copy waited for the whole send.
    const size_t bytes = Deki::FrameBufferBytes(Deki::ColorFormat::RGB565, m_DisplayWidth, m_DisplayHeight);
    for (uint8_t*& staging : m_Staging)
    {
        staging = static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    m_FrameReady = xQueueCreate(2, sizeof(int));
    m_FreeStaging = xSemaphoreCreateCounting(2, 2);
    TaskHandle_t task = nullptr;
    // The other core from the one setting this up, which runs the engine.
    const BaseType_t core = xPortGetCoreID() == 0 ? 1 : 0;
    if (m_Staging[0] && m_Staging[1] && m_FrameReady && m_FreeStaging)
    {
        m_StagingBytes = bytes;
        if (xTaskCreatePinnedToCore(&PresentTaskMain, "deki_present", 4096, this, 5, &task, core) == pdPASS)
        {
            m_PresentTask = task;
            m_AsyncPresent = true;
            DEKI_LOG_INFO("LovyanGFX: frames are sent from core %d while the next is drawn", (int)core);
            return true;
        }
    }
    DEKI_LOG_ERROR("LovyanGFX: cannot send frames from the other core (no external memory for two %u-byte copies, or "
                   "no task); they are sent as they are drawn",
                   (unsigned)bytes);
    SetAsyncPresent(false);
    return false;
#else
    DEKI_LOG_WARNING("LovyanGFX: a single-core chip cannot send frames while drawing; they are sent as they are drawn");
    return false;
#endif
}

// The task on the other core: sends each frame Present copies for it.
void LovyanGFXDisplay::PresentTaskMain(void* arg)
{
    auto* self = static_cast<LovyanGFXDisplay*>(arg);
    uint32_t frames = 0;
    for (;;)
    {
        // At full speed the next frame is always waiting, so this task never
        // blocks and the core's idle task never runs, which the task watchdog
        // reports. A tick's pause every 30 frames lets it.
        if (++frames % 30 == 0)
        {
            vTaskDelay(1);
        }
        int index = 0;
        xQueueReceive(static_cast<QueueHandle_t>(self->m_FrameReady), &index, portMAX_DELAY);
        self->PushRows(self->m_Staging[index], self->m_StagingWidth, self->m_StagingHeight, self->m_StagingFormat, 0,
                       self->m_StagingHeight);
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(self->m_FreeStaging));
    }
}

void LovyanGFXDisplay::WaitForAsyncPresent()
{
    if (!m_AsyncPresent)
    {
        return;
    }
    // Both copies free: nothing queued or being sent.
    auto* freeStaging = static_cast<SemaphoreHandle_t>(m_FreeStaging);
    xSemaphoreTake(freeStaging, portMAX_DELAY);
    xSemaphoreTake(freeStaging, portMAX_DELAY);
    xSemaphoreGive(freeStaging);
    xSemaphoreGive(freeStaging);
}

bool LovyanGFXDisplay::CanPresentAsync(const uint8_t* framebuffer, int width, int height,
                                       Deki::ColorFormat format) const
{
    // The passthrough, full-frame path below, for a panel that takes RGB565
    // big-endian (byte-swapped from a little-endian build, as it is from a
    // big-endian one), and nothing else.
    return m_AsyncPresent && framebuffer && format == Deki::ColorFormat::RGB565 && !buffers[m_RenderIndex] &&
           width == m_DisplayWidth && height == m_DisplayHeight && (m_SwapBytes || Deki::kRGB565BigEndian) &&
           !(m_ActiveOverlay && m_ActiveOverlay->buffer) &&
           Deki::FrameBufferBytes(format, width, height) <= m_StagingBytes;
}

void LovyanGFXDisplay::Present(const uint8_t* framebuffer, int width, int height, Deki::ColorFormat format)
{
    if (!initialized || !framebuffer)
    {
        return;
    }

    ConvertAndRenderFramebuffer(framebuffer, width, height, format);
}

bool LovyanGFXDisplay::SupportsPartialPresent() const
{
    return true;
}

// ---- Partial present ------------------------------------------------------
// Not yet tested on hardware: written against the LovyanGFX API the full path
// above uses (pushImage / startWrite / endWrite / waitDMA) and only
// compile-checked. Run a scene with dirty-rect tracking on to try it.

bool LovyanGFXDisplay::EnsureBands()
{
    if (m_Band[0] && m_Band[1])
    {
        return true;
    }
    const size_t bytes = static_cast<size_t>(m_DisplayWidth) * kBandRows * sizeof(uint16_t);
    for (int i = 0; i < 2; ++i)
    {
        if (m_Band[i])
        {
            continue;
        }
        // The panel reads band buffers by DMA, like the framebuffer.
        m_Band[i] = static_cast<uint16_t*>(Deki::Memory::AllocateDma(bytes, Deki::Memory::Internal));
        if (!m_Band[i])
        {
            DEKI_LOG_ERROR("LovyanGFX: cannot allocate a %zu-byte staging band; partial present off", bytes);
            FreeBands();
            return false;
        }
    }
    return true;
}

void LovyanGFXDisplay::FreeBands()
{
    for (int i = 0; i < 2; ++i)
    {
        Deki::Memory::Free(m_Band[i]);
        m_Band[i] = nullptr;
    }
}

static inline uint16_t SwapBytes16(uint16_t v)
{
    return static_cast<uint16_t>((v >> 8) | (v << 8));
}

// An overlay pixel (ARGB8888, alpha not 0) over an RGB565 one, both values in
// the CPU's byte order.
static inline uint16_t BlendOverlayPixel(uint16_t bg, uint32_t argb)
{
    if ((argb & 0xFF000000) == 0xFF000000)
    {
        return static_cast<uint16_t>(((argb >> 8) & 0xF800) | ((argb >> 5) & 0x07E0) | ((argb >> 3) & 0x001F));
    }
    const uint8_t alpha = argb >> 24;
    const uint8_t r = (argb >> 16) & 0xFF;
    const uint8_t g = (argb >> 8) & 0xFF;
    const uint8_t b = argb & 0xFF;

    const uint8_t bgR = (bg >> 8) & 0xF8;
    const uint8_t bgG = (bg >> 3) & 0xFC;
    const uint8_t bgB = (bg << 3) & 0xF8;

    const uint8_t invAlpha = 255 - alpha;
    const uint8_t outR = (r * alpha + bgR * invAlpha + 128) >> 8;
    const uint8_t outG = (g * alpha + bgG * invAlpha + 128) >> 8;
    const uint8_t outB = (b * alpha + bgB * invAlpha + 128) >> 8;

    return static_cast<uint16_t>(((outR & 0xF8) << 8) | ((outG & 0xFC) << 3) | (outB >> 3));
}

void LovyanGFXDisplay::PushRows(const uint8_t* framebuffer, int width, int height, Deki::ColorFormat format, int y0,
                                int y1)
{
    if (!EnsureBands())
    {
        return;
    }
    const int w = (width < m_DisplayWidth) ? width : m_DisplayWidth;
    y0 = y0 < 0 ? 0 : y0;
    y1 = y1 > height ? height : y1;
    y1 = y1 > m_DisplayHeight ? m_DisplayHeight : y1;

    // One transaction for all the bands, so each goes out by DMA while the CPU
    // fills the other: band N is converted while band N-1 is still being
    // sent, which it reads from the other buffer. Ending the transaction
    // waits for the last band (Panel_LCD::end_transaction), so the bands are
    // free again when this returns. Separate transactions per band would wait
    // for each band before converting the next, the conversion and the
    // transfer one after the other.
    tft->startWrite();
    for (int y = y0; y < y1; y += kBandRows)
    {
        const int rows = (y1 - y < kBandRows) ? (y1 - y) : kBandRows;
        uint16_t* band = m_Band[m_BandIndex];

        for (int i = 0; i < rows; ++i)
        {
            uint16_t* dst = band + static_cast<size_t>(i) * w;
            const int sy = y + i;
            if (format == Deki::ColorFormat::RGB565)  // RGB565
            {
                const uint16_t* src = reinterpret_cast<const uint16_t*>(framebuffer) + static_cast<size_t>(sy) * width;
                if (m_SwapBytes != Deki::kRGB565BigEndian)  // the panel takes the other byte order
                {
                    for (int x = 0; x < w; ++x)
                    {
                        dst[x] = SwapBytes16(src[x]);
                    }
                }
                else
                {
                    memcpy(dst, src, static_cast<size_t>(w) * sizeof(uint16_t));
                }
            }
            else if (format == Deki::ColorFormat::ARGB8888)  // ARGB8888
            {
                const uint32_t* src = reinterpret_cast<const uint32_t*>(framebuffer) + static_cast<size_t>(sy) * width;
                for (int x = 0; x < w; ++x)
                {
                    const uint32_t p = src[x];
                    const uint16_t v =
                        static_cast<uint16_t>((((p >> 16) & 0xF8) << 8) | (((p >> 8) & 0xFC) << 3) | ((p & 0xFF) >> 3));
                    dst[x] = m_SwapBytes ? SwapBytes16(v) : v;
                }
            }
            else if (format == Deki::ColorFormat::RGB888)  // RGB888
            {
                const uint8_t* src = framebuffer + static_cast<size_t>(sy) * width * 3;
                for (int x = 0; x < w; ++x)
                {
                    const uint16_t v = static_cast<uint16_t>(((src[x * 3] & 0xF8) << 8) |
                                                             ((src[x * 3 + 1] & 0xFC) << 3) | (src[x * 3 + 2] >> 3));
                    dst[x] = m_SwapBytes ? SwapBytes16(v) : v;
                }
            }
            else
            {
                memset(dst, 0, static_cast<size_t>(w) * sizeof(uint16_t));
            }
        }

        // Waits for the band before (setting the window waits for the bus),
        // then starts this one and returns while it is sent.
        tft->pushImageDMA(0, y, w, rows, band);
        m_BandIndex = 1 - m_BandIndex;
    }
    tft->endWrite();
    m_DmaInFlight = false;
}

void LovyanGFXDisplay::FinishPresent()
{
    if (m_DoubleBuffer)
    {
        // The engine renders the next frame into the other buffer while the
        // last band's DMA finishes. The bands are private, so this is safe
        // even in direct-render mode.
        m_RenderIndex = 1 - m_RenderIndex;
    }
    else if (m_DmaInFlight)
    {
        tft->waitDMA();
        m_DmaInFlight = false;
    }
}

void LovyanGFXDisplay::PresentRegions(const uint8_t* framebuffer, int width, int height, Deki::ColorFormat format,
                                      const Deki::Rect* rects, int32_t count)
{
    if (!initialized || !framebuffer)
    {
        return;
    }
    WaitForAsyncPresent();
    if (count == 0)
    {
        return;  // nothing changed on the panel
    }

    // The overlay is composited over the whole frame by the full path.
    if (m_ActiveOverlay && m_ActiveOverlay->buffer)
    {
        Present(framebuffer, width, height, format);
        return;
    }

    // pushImage takes a packed rectangle, and the framebuffer's rows are only
    // contiguous at full width, so rectangles become row bands.
    m_BandScratch.clear();
    for (int32_t i = 0; i < count; ++i)
    {
        const Deki::Rect& r = rects[i];
        if (r.Empty())
        {
            continue;
        }
        // Widen to the panel's row alignment. PushRows clamps to the frame,
        // whose height on such a panel is a multiple of it.
        const int32_t top = r.top - (r.top % m_RowAlign);
        const int32_t bottom = ((r.bottom + m_RowAlign - 1) / m_RowAlign) * m_RowAlign;
        m_BandScratch.push_back(Deki::Rect{ 0, top, width, bottom });
    }
    std::sort(m_BandScratch.begin(), m_BandScratch.end(),
              [](const Deki::Rect& a, const Deki::Rect& b) { return a.top < b.top; });

    size_t out = 0;
    for (size_t i = 0; i < m_BandScratch.size(); ++i)
    {
        if (out > 0 && m_BandScratch[i].top <= m_BandScratch[out - 1].bottom)
        {
            if (m_BandScratch[i].bottom > m_BandScratch[out - 1].bottom)
            {
                m_BandScratch[out - 1].bottom = m_BandScratch[i].bottom;
            }
        }
        else
        {
            m_BandScratch[out++] = m_BandScratch[i];
        }
    }
    m_BandScratch.resize(out);

    for (const Deki::Rect& band : m_BandScratch)
    {
        PushRows(framebuffer, width, height, format, band.top, band.bottom);
    }

    FinishPresent();
}

void LovyanGFXDisplay::ConvertAndRenderFramebuffer(const uint8_t* framebuffer, int width, int height,
                                                   Deki::ColorFormat format)
{
    // Async: copy the frame into a free staging copy and queue it for the
    // task on the other core, then return, so the engine draws the next frame
    // while this one is sent. Waits only when both copies are in use.
    if (CanPresentAsync(framebuffer, width, height, format))
    {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(m_FreeStaging), portMAX_DELAY);
        const int index = m_NextStaging;
        m_NextStaging = 1 - m_NextStaging;
        memcpy(m_Staging[index], framebuffer, Deki::FrameBufferBytes(format, width, height));
        m_StagingWidth = width;
        m_StagingHeight = height;
        m_StagingFormat = format;
        xQueueSend(static_cast<QueueHandle_t>(m_FrameReady), &index, portMAX_DELAY);
        return;
    }
    // Any other present goes after the frames the task has: it uses the bus
    // and the bands.
    WaitForAsyncPresent();
    uint16_t* conversionBuffer = buffers[m_RenderIndex];

    // Passthrough mode: no display buffer; push the framebuffer directly (RGB565 only).
    const bool passthrough = (!conversionBuffer && format == Deki::ColorFormat::RGB565);
    if (passthrough && (width != m_DisplayWidth || height != m_DisplayHeight))
    {
        // pushImage below reads the framebuffer as if it were panel-sized: a
        // larger one would shear, a smaller one would be read past its end.
        // The engine sizes the framebuffer to this panel, so this only guards
        // a mismatch; the staging bands copy row by row and clamp to both.
        PushRows(framebuffer, width, height, format, 0, height);
        FinishPresent();
        return;
    }
    if (passthrough)
    {
        conversionBuffer = (uint16_t*)framebuffer;
    }
    else if (!conversionBuffer)
    {
        DEKI_LOG_ERROR("LovyanGFX: Buffer not allocated and format is not RGB565");
        return;
    }

    // Log the first Present call.
    static int presentCount = 0;
    if (presentCount == 0)
    {
        DEKI_LOG_INTERNAL(
            "LovyanGFX First Present: fmt=%d size=%dx%d overlay=%s doubleBuffer=%d psram=%d passthrough=%d", format,
            width, height, (m_ActiveOverlay && m_ActiveOverlay->buffer) ? "YES" : "NO", m_DoubleBuffer ? 1 : 0,
            m_UsePSRAM ? 1 : 0, passthrough ? 1 : 0);
    }
    presentCount++;

    // Fast path: the framebuffer is the output buffer (direct rendering or passthrough).
    const bool directBuffer =
        passthrough || (format == Deki::ColorFormat::RGB565 && (const uint16_t*)framebuffer == conversionBuffer);

    if (directBuffer)
    {
        if (presentCount == 1)
        {
            DEKI_LOG_INTERNAL("LovyanGFX: Direct render buffer - skipping memcpy");
        }
    }

    if (!directBuffer)
    {
        // Flush the source framebuffer from the CPU cache if it is in PSRAM.
#if defined(ESP32) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        if (esp_ptr_external_ram(framebuffer))
        {
            uintptr_t addr = (uintptr_t)framebuffer;
            uintptr_t alignedAddr = addr & ~63;
            const size_t rawBytes = Deki::FrameBufferBytes(format, width, height);
            size_t alignedBytes = ((addr - alignedAddr) + rawBytes + 63) & ~63;
            esp_cache_msync((void*)alignedAddr, alignedBytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        }
#endif

        // Conversion with direct pointer arithmetic.
        int effectiveWidth = (width < m_DisplayWidth) ? width : m_DisplayWidth;
        int effectiveHeight = (height < m_DisplayHeight) ? height : m_DisplayHeight;
        size_t pixelCount = effectiveWidth * effectiveHeight;

        if (format == Deki::ColorFormat::ARGB8888)  // the most common path
        {
            const uint32_t* src = (const uint32_t*)framebuffer;
            uint16_t* dst = conversionBuffer;

            if (width == m_DisplayWidth && height == m_DisplayHeight)
            {
                for (size_t i = 0; i < pixelCount; i++)
                {
                    uint32_t pixel = src[i];
                    uint8_t b = pixel & 0xFF;
                    uint8_t g = (pixel >> 8) & 0xFF;
                    uint8_t r = (pixel >> 16) & 0xFF;
                    dst[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                }
            }
            else
            {
                for (int y = 0; y < effectiveHeight; y++)
                {
                    const uint32_t* srcRow = src + y * width;
                    uint16_t* dstRow = dst + y * m_DisplayWidth;

                    for (int x = 0; x < effectiveWidth; x++)
                    {
                        uint32_t pixel = srcRow[x];
                        uint8_t b = pixel & 0xFF;
                        uint8_t g = (pixel >> 8) & 0xFF;
                        uint8_t r = (pixel >> 16) & 0xFF;
                        dstRow[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                    }
                }
            }
        }
        else if (format == Deki::ColorFormat::RGB565)  // RGB565
        {
            const uint16_t* src = (const uint16_t*)framebuffer;
            uint16_t* dst = conversionBuffer;

            if (width == m_DisplayWidth && height == m_DisplayHeight)
            {
                memcpy(dst, src, pixelCount * sizeof(uint16_t));
            }
            else
            {
                for (int y = 0; y < effectiveHeight; y++)
                {
                    memcpy(dst + y * m_DisplayWidth, src + y * width, effectiveWidth * sizeof(uint16_t));
                }
            }
        }
        else if (format == Deki::ColorFormat::RGB888)  // RGB888
        {
            const uint8_t* src = framebuffer;
            uint16_t* dst = conversionBuffer;

            for (int y = 0; y < effectiveHeight; y++)
            {
                const uint8_t* srcRow = src + y * width * 3;
                uint16_t* dstRow = dst + y * m_DisplayWidth;

                for (int x = 0; x < effectiveWidth; x++)
                {
                    uint8_t r = srcRow[x * 3 + 0];
                    uint8_t g = srcRow[x * 3 + 1];
                    uint8_t b = srcRow[x * 3 + 2];
                    uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                    dstRow[x] = rgb565;
                }
            }
        }
        else
        {
            // Unknown format: fill with black.
            memset(conversionBuffer, 0, pixelCount * sizeof(uint16_t));
        }

    }  // if (!directBuffer)

    // RGB565 copied from the framebuffer, or the framebuffer itself, is in
    // memory's byte order (Deki::kRGB565BigEndian); a conversion from another
    // format is in the CPU's.
    const bool bufferBigEndian = Deki::kRGB565BigEndian && format == Deki::ColorFormat::RGB565;

    // Composite the active UI overlay (ARGB8888) on top.
    if (m_ActiveOverlay && m_ActiveOverlay->buffer)
    {
        const int overlayWidth = m_ActiveOverlay->width < m_DisplayWidth ? m_ActiveOverlay->width : m_DisplayWidth;
        const int overlayHeight =
            m_ActiveOverlay->height < m_DisplayHeight ? m_ActiveOverlay->height : m_DisplayHeight;

        for (int y = 0; y < overlayHeight; y++)
        {
            const uint32_t* overlayRow = m_ActiveOverlay->buffer + y * m_ActiveOverlay->width;
            uint16_t* dstRow = conversionBuffer + y * m_DisplayWidth;
            for (int x = 0; x < overlayWidth; x++)
            {
                const uint32_t argb = overlayRow[x];
                if ((argb & 0xFF000000) == 0)
                {
                    continue;
                }
                const uint16_t bg = bufferBigEndian ? SwapBytes16(dstRow[x]) : dstRow[x];
                const uint16_t out = BlendOverlayPixel(bg, argb);
                dstRow[x] = bufferBigEndian ? SwapBytes16(out) : out;
            }
        }
    }

    // A direct or passthrough buffer belongs to the engine: it renders into it
    // and blends against its contents next frame, so a byte swap for the
    // panel must not happen in place. Push through the staging bands, which
    // swap while copying.
    if (directBuffer && bufferBigEndian != m_SwapBytes)
    {
        PushRows(framebuffer, width, height, format, 0, height);
        FinishPresent();
        return;
    }

    // Bulk byte swap when the panel takes the other byte order. One tight loop
    // is faster than swapping per pixel while rendering or using LovyanGFX's
    // pixelcopy path.
    if (bufferBigEndian != m_SwapBytes)
    {
        uint32_t* buf32 = (uint32_t*)conversionBuffer;
        size_t count32 = m_BufferPixelCount / 2;
        for (size_t i = 0; i < count32; i++)
        {
            uint32_t v = buf32[i];
            buf32[i] = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
        }
    }

    // Flush the PSRAM display buffer from the CPU cache before DMA reads it,
    // after the last write to it.
#if defined(ESP32) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (m_UsePSRAM)
    {
        uintptr_t addr = (uintptr_t)conversionBuffer;
        uintptr_t alignedAddr = addr & ~63;
        size_t rawBytes = m_BufferPixelCount * sizeof(uint16_t);
        size_t alignedBytes = ((addr - alignedAddr) + rawBytes + 63) & ~63;
        esp_cache_msync((void*)alignedAddr, alignedBytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }
#endif

    if (m_DoubleBuffer)
    {
        if (m_DmaInFlight)
        {
            tft->waitDMA();
        }

        tft->startWrite();
        tft->pushImage(0, 0, m_DisplayWidth, m_DisplayHeight, conversionBuffer);
        tft->endWrite();
        m_DmaInFlight = true;

        m_RenderIndex = 1 - m_RenderIndex;
    }
    else
    {
        tft->startWrite();
        tft->pushImage(0, 0, m_DisplayWidth, m_DisplayHeight, conversionBuffer);
        tft->endWrite();
        tft->waitDMA();
    }
}

void LovyanGFXDisplay::GetDisplaySize(int32_t* width, int32_t* height) const
{
    if (width)
    {
        *width = m_DisplayWidth;
    }
    if (height)
    {
        *height = m_DisplayHeight;
    }
}

bool LovyanGFXDisplay::IsInitialized() const
{
    return initialized;
}

void LovyanGFXDisplay::RequestFullRefresh()
{
    // Every Present refreshes the whole screen, so there is nothing to do.
}

bool LovyanGFXDisplay::ProcessEvents()
{
    // There are no window events on embedded platforms.
    return true;
}

void* LovyanGFXDisplay::CreateUIOverlay(int32_t width, int32_t height)
{
    if (!initialized)
    {
        DEKI_LOG_ERROR("LovyanGFXDisplay::CreateUIOverlay: Display not initialized");
        return nullptr;
    }

    UIOverlay* overlay = new UIOverlay();
    if (!overlay)
    {
        DEKI_LOG_ERROR("LovyanGFXDisplay::CreateUIOverlay: Failed to allocate overlay structure");
        return nullptr;
    }

    overlay->width = width;
    overlay->height = height;

    size_t bufferSize = width * height * sizeof(uint32_t);
    overlay->buffer = (uint32_t*)Deki::Memory::Allocate(bufferSize, Deki::Memory::External);

    if (!overlay->buffer)
    {
        DEKI_LOG_ERROR("LovyanGFXDisplay::CreateUIOverlay: Failed to allocate overlay buffer (%zu bytes)", bufferSize);
        delete overlay;
        return nullptr;
    }

    memset(overlay->buffer, 0, bufferSize);

    DEKI_LOG_INTERNAL("LovyanGFXDisplay: Created UI overlay %dx%d (%zu bytes, ARGB8888 format)", width, height,
                      bufferSize);
    return overlay;
}

bool LovyanGFXDisplay::UpdateUIOverlay(void* overlay, int32_t x, int32_t y, int32_t width, int32_t height,
                                       const uint32_t* buffer)
{
    if (!overlay || !buffer)
    {
        return false;
    }

    UIOverlay* uiOverlay = (UIOverlay*)overlay;

    if (x < 0 || y < 0 || x + width > uiOverlay->width || y + height > uiOverlay->height)
    {
        DEKI_LOG_WARNING("LovyanGFXDisplay::UpdateUIOverlay: Invalid bounds (%d,%d,%d,%d) for overlay %dx%d", x, y,
                         width, height, uiOverlay->width, uiOverlay->height);
        return false;
    }

    for (int32_t row = 0; row < height; row++)
    {
        uint32_t* dest = &uiOverlay->buffer[(y + row) * uiOverlay->width + x];
        const uint32_t* src = &buffer[row * width];
        memcpy(dest, src, width * sizeof(uint32_t));
    }

    return true;
}

bool LovyanGFXDisplay::UpdateUIOverlayRGB565A8(void* overlay, int32_t x, int32_t y, int32_t width, int32_t height,
                                               const uint8_t* rgb565a8Pixels)
{
    return false;
}

void LovyanGFXDisplay::DestroyUIOverlay(void* overlay)
{
    if (!overlay)
    {
        return;
    }

    UIOverlay* uiOverlay = (UIOverlay*)overlay;

    if (uiOverlay == m_ActiveOverlay)
    {
        m_ActiveOverlay = nullptr;
    }

    if (uiOverlay->buffer)
    {
        Deki::Memory::Free(uiOverlay->buffer);
        uiOverlay->buffer = nullptr;
    }

    delete uiOverlay;

    DEKI_LOG_INTERNAL("LovyanGFXDisplay: Destroyed UI overlay");
}

void LovyanGFXDisplay::SetActiveUIOverlay(void* overlay)
{
    m_ActiveOverlay = (UIOverlay*)overlay;

    if (m_ActiveOverlay)
    {
        DEKI_LOG_INTERNAL("LovyanGFXDisplay: Set active UI overlay %dx%d", m_ActiveOverlay->width,
                          m_ActiveOverlay->height);
    }
    else
    {
        DEKI_LOG_INTERNAL("LovyanGFXDisplay: Cleared active UI overlay");
    }
}

void LovyanGFXDisplay::ClearActiveUIOverlay()
{
    if (!m_ActiveOverlay || !m_ActiveOverlay->buffer)
    {
        return;
    }

    size_t bufferSize = m_ActiveOverlay->width * m_ActiveOverlay->height * sizeof(uint32_t);
    memset(m_ActiveOverlay->buffer, 0, bufferSize);
}

uint8_t* LovyanGFXDisplay::GetRenderBuffer(int32_t* width, int32_t* height)
{
    // Offered wherever it lives. With usePsram the engine draws straight into
    // PSRAM: blits are slower than in internal RAM, but a screen-sized buffer
    // in internal RAM leaves too little for the scene on a 320x240 panel and
    // does not fit at all on larger ones.
    if (!initialized || !buffers[m_RenderIndex])
    {
        return nullptr;
    }
    if (width)
    {
        *width = m_DisplayWidth;
    }
    if (height)
    {
        *height = m_DisplayHeight;
    }
    return (uint8_t*)buffers[m_RenderIndex];
}

void LovyanGFXDisplay::SetBacklight(bool on)
{
    if (!tft)
    {
        return;
    }
    tft->setBrightness(on ? 255 : 0);
}

#else
// Stub for builds other than ESP32.
LovyanGFXDisplay::LovyanGFXDisplay()
    : tft(nullptr),
      m_DisplayWidth(0),
      m_DisplayHeight(0),
      initialized(false),
      buffers{ nullptr, nullptr },
      m_BufferPixelCount(0),
      m_RenderIndex(0),
      m_DmaInFlight(false),
      m_UsePSRAM(false),
      m_DoubleBuffer(false),
      m_SwapBytes(false),
      m_ActiveOverlay(nullptr)
{
}
LovyanGFXDisplay::~LovyanGFXDisplay()
{
}
bool LovyanGFXDisplay::InitializeWithDevice(lgfx::LGFX_Device*, int32_t, int32_t, bool, bool, bool)
{
    return false;
}
bool LovyanGFXDisplay::Initialize(int32_t width, int32_t height)
{
    return false;
}
void LovyanGFXDisplay::Shutdown()
{
}
void LovyanGFXDisplay::Present(const uint8_t* framebuffer, int width, int height, Deki::ColorFormat format)
{
}
bool LovyanGFXDisplay::SetAsyncPresent(bool)
{
    return false;
}
void LovyanGFXDisplay::PresentTaskMain(void*)
{
}
void LovyanGFXDisplay::WaitForAsyncPresent()
{
}
bool LovyanGFXDisplay::CanPresentAsync(const uint8_t*, int, int, Deki::ColorFormat) const
{
    return false;
}
bool LovyanGFXDisplay::SupportsPartialPresent() const
{
    return false;
}
void LovyanGFXDisplay::PresentRegions(const uint8_t*, int, int, Deki::ColorFormat, const Deki::Rect*, int32_t)
{
}
bool LovyanGFXDisplay::EnsureBands()
{
    return false;
}
void LovyanGFXDisplay::FreeBands()
{
}
void LovyanGFXDisplay::PushRows(const uint8_t*, int, int, Deki::ColorFormat, int, int)
{
}
void LovyanGFXDisplay::FinishPresent()
{
}
void LovyanGFXDisplay::ConvertAndRenderFramebuffer(const uint8_t* framebuffer, int width, int height,
                                                   Deki::ColorFormat format)
{
}
void LovyanGFXDisplay::GetDisplaySize(int32_t* width, int32_t* height) const
{
}
bool LovyanGFXDisplay::IsInitialized() const
{
    return false;
}
void LovyanGFXDisplay::RequestFullRefresh()
{
}
bool LovyanGFXDisplay::ProcessEvents()
{
    return true;
}
void* LovyanGFXDisplay::CreateUIOverlay(int32_t width, int32_t height)
{
    return nullptr;
}
bool LovyanGFXDisplay::UpdateUIOverlay(void* overlay, int32_t x, int32_t y, int32_t width, int32_t height,
                                       const uint32_t* buffer)
{
    return false;
}
bool LovyanGFXDisplay::UpdateUIOverlayRGB565A8(void* overlay, int32_t x, int32_t y, int32_t width, int32_t height,
                                               const uint8_t* rgb565a8Pixels)
{
    return false;
}
void LovyanGFXDisplay::DestroyUIOverlay(void* overlay)
{
}
void LovyanGFXDisplay::SetActiveUIOverlay(void* overlay)
{
}
void LovyanGFXDisplay::ClearActiveUIOverlay()
{
}
uint8_t* LovyanGFXDisplay::GetRenderBuffer(int32_t*, int32_t*)
{
    return nullptr;
}
void LovyanGFXDisplay::SetBacklight(bool)
{
}
#endif

}  // namespace DekiLovyanGfx
