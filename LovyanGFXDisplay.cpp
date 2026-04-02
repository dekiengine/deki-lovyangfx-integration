#include "LovyanGFXDisplay.h"

#ifdef ESP32
#include <cstring>

#include "DekiLogSystem.h"
#include "providers/DekiMemoryProvider.h"
#include "../rendering/QuadBlit.h"

// ESP32-specific includes for DMA memory allocation and cache management
#ifdef ESP32
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_task_wdt.h>  // For watchdog feeding during long display writes
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <esp_cache.h>
#include <esp_dma_utils.h>  // For esp_dma_malloc (ensures DMA + cache alignment)
#endif
#endif

#include <LovyanGFX.hpp>

// Helper: conditionally swap RGB565 bytes based on runtime display configuration
static inline uint16_t PrepareRGB565ForDisplay(uint16_t rgb565, bool swap)
{
    if (swap)
        return (rgb565 >> 8) | (rgb565 << 8);
    return rgb565;
}

LovyanGFXDisplay::LovyanGFXDisplay()
: tft(nullptr)
, display_width(320)
, display_height(240)
, initialized(false)
, m_SwapBytes(false)
, buffers{nullptr, nullptr}
, buffer_pixel_count(0)
, render_index(0)
, dma_in_flight(false)
, m_UsePSRAM(false)
, m_DoubleBuffer(false)
, active_overlay(nullptr)
{
}

LovyanGFXDisplay::~LovyanGFXDisplay()
{
    Shutdown();
}

static uint16_t* AllocateDisplayBuffer(size_t buffer_bytes, bool usePSRAM, const char* label)
{
    uint16_t* buf = nullptr;

#ifdef ESP32
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    // esp_dma_malloc ensures cache-line alignment (required for esp_cache_msync)
    // and works for both internal RAM and PSRAM on ESP32-S3
    if (usePSRAM)
    {
        // Allocate cache-aligned buffer in PSRAM
        // esp_dma_malloc with MALLOC_CAP_SPIRAM ensures proper alignment
        buf = (uint16_t*)heap_caps_aligned_alloc(64, buffer_bytes, MALLOC_CAP_SPIRAM);
        if (buf)
        {
            DEKI_LOG_DEBUG("LovyanGFX: Allocated %s in PSRAM cache-aligned (%zu bytes)", label, buffer_bytes);
        }
    }
    else
    {
        esp_err_t dma_result = esp_dma_malloc(buffer_bytes, 0, (void**)&buf, NULL);
        if (dma_result == ESP_OK && buf)
        {
            DEKI_LOG_DEBUG("LovyanGFX: Allocated %s DMA+cache-aligned (%zu bytes)", label, buffer_bytes);
        }
        else
        {
            buf = (uint16_t*)heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
            if (buf)
            {
                DEKI_LOG_WARNING("LovyanGFX: esp_dma_malloc failed for %s, using heap_caps_malloc (%zu bytes)", label, buffer_bytes);
            }
        }
    }
#else
    if (usePSRAM)
    {
        buf = (uint16_t*)heap_caps_malloc(buffer_bytes, MALLOC_CAP_SPIRAM);
        if (buf)
        {
            DEKI_LOG_DEBUG("LovyanGFX: Allocated %s in PSRAM (%zu bytes)", label, buffer_bytes);
        }
    }
    else
    {
        buf = (uint16_t*)heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (buf)
        {
            DEKI_LOG_DEBUG("LovyanGFX: Allocated %s in internal RAM (%zu bytes)", label, buffer_bytes);
        }
    }
#endif

    if (!buf)
    {
        buf = (uint16_t*)malloc(buffer_bytes);
        if (buf)
        {
            DEKI_LOG_WARNING("LovyanGFX: Fallback malloc for %s (%zu bytes)", label, buffer_bytes);
        }
    }
#else
    buf = (uint16_t*)malloc(buffer_bytes);
#endif

    return buf;
}

bool LovyanGFXDisplay::InitializeWithDevice(lgfx::LGFX_Device* device, int32_t width, int32_t height,
                                             bool swapBytes, bool usePSRAM, bool doubleBuffer)
{
    if (initialized)
        return true;

    if (!device)
    {
        DEKI_LOG_ERROR("LovyanGFXDisplay::InitializeWithDevice: device is null");
        return false;
    }

    tft = device;
    display_width = width;
    display_height = height;
    m_SwapBytes = swapBytes;
    m_UsePSRAM = usePSRAM;
    m_DoubleBuffer = doubleBuffer;
    QuadBlit::SetByteSwap(swapBytes);

    buffer_pixel_count = width * height;
    size_t buffer_bytes = buffer_pixel_count * sizeof(uint16_t);

    // Allocate primary buffer
    buffers[0] = AllocateDisplayBuffer(buffer_bytes, usePSRAM, "buffer[0]");
    if (!buffers[0])
    {
        DEKI_LOG_ERROR("LovyanGFX: Failed to allocate primary buffer (%zu bytes)", buffer_bytes);
        return false;
    }
    memset(buffers[0], 0, buffer_bytes);

    // Allocate secondary buffer for double buffering (async DMA)
    if (doubleBuffer)
    {
        buffers[1] = AllocateDisplayBuffer(buffer_bytes, usePSRAM, "buffer[1]");
        if (!buffers[1])
        {
            DEKI_LOG_WARNING("LovyanGFX: Failed to allocate second buffer, falling back to single-buffer mode");
            m_DoubleBuffer = false;
        }
        else
        {
            memset(buffers[1], 0, buffer_bytes);
        }
    }

    render_index = 0;
    dma_in_flight = false;

    initialized = true;
    DEKI_LOG_DEBUG("LovyanGFX display initialized %dx%d (swap=%d, psram=%d, double_buffer=%d)",
                   width, height, swapBytes ? 1 : 0, usePSRAM ? 1 : 0, m_DoubleBuffer ? 1 : 0);

    return true;
}

bool LovyanGFXDisplay::Initialize(int32_t width, int32_t height)
{
    // No longer auto-creates device — use InitializeWithDevice() via LGFXDisplayPanel
    DEKI_LOG_ERROR("LovyanGFXDisplay::Initialize() called directly — use LGFXDisplayPanel component instead");
    return false;
}

void LovyanGFXDisplay::Shutdown()
{
    if (!initialized)
    {
        return;
    }

    // Wait for any in-flight DMA before freeing buffers
    if (dma_in_flight && tft)
    {
        tft->waitDMA();
        dma_in_flight = false;
    }

    for (int i = 0; i < 2; i++)
    {
        if (buffers[i])
        {
#ifdef ESP32
            heap_caps_free(buffers[i]);
#else
            free(buffers[i]);
#endif
            buffers[i] = nullptr;
        }
    }
    buffer_pixel_count = 0;
    DEKI_LOG_DEBUG("LovyanGFX: Freed display buffers");

    initialized = false;
}

void LovyanGFXDisplay::Present(const uint8_t* framebuffer, int width, int height, int format)
{
    if (!initialized || !framebuffer)
    {
        return;
    }

    ConvertAndRenderFramebuffer(framebuffer, width, height, format);
}

void LovyanGFXDisplay::ConvertAndRenderFramebuffer(const uint8_t* framebuffer, int width, int height, int format)
{
    uint16_t* conversion_buffer = buffers[render_index];
    if (!conversion_buffer)
    {
        DEKI_LOG_ERROR("LovyanGFX: Buffer not allocated");
        return;
    }

    // Debug: Log first Present call
    static int present_count = 0;
    if (present_count == 0)
    {
        DEKI_LOG_DEBUG("LovyanGFX First Present: fmt=%d size=%dx%d overlay=%s swap=%d double_buffer=%d psram=%d",
                     format, width, height,
                     (active_overlay && active_overlay->buffer) ? "YES" : "NO",
                     m_SwapBytes ? 1 : 0, m_DoubleBuffer ? 1 : 0, m_UsePSRAM ? 1 : 0);
    }
    present_count++;

    const bool swap = m_SwapBytes;

    // Fast path: framebuffer IS the current render buffer (direct rendering, no copy needed)
    const bool directBuffer = (format == 0 && (const uint16_t*)framebuffer == conversion_buffer);

    if (directBuffer)
    {
        if (present_count == 1)
        {
            DEKI_LOG_DEBUG("LovyanGFX: Direct render buffer — skipping memcpy");
        }
    }

    if (!directBuffer)
    {

    // CRITICAL: Flush PSRAM framebuffer from CPU cache to RAM BEFORE we read from it
#if defined(ESP32) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    {
        size_t bytes_per_pixel = (format == 2) ? 4 : (format == 1) ? 3 : 2;
        size_t framebuffer_size = width * height * bytes_per_pixel;
        esp_cache_msync((void*)framebuffer, framebuffer_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }
#endif

    // Optimized conversion using direct pointer arithmetic
    int effective_width = (width < display_width) ? width : display_width;
    int effective_height = (height < display_height) ? height : display_height;
    size_t pixel_count = effective_width * effective_height;

    if (format == 2)  // ARGB8888 - most common path
    {
        const uint32_t* src = (const uint32_t*)framebuffer;
        uint16_t* dst = conversion_buffer;

        if (width == display_width && height == display_height)
        {
            if (swap)
            {
                for (size_t i = 0; i < pixel_count; i++)
                {
                    uint32_t pixel = src[i];
                    uint8_t b = pixel & 0xFF;
                    uint8_t g = (pixel >> 8) & 0xFF;
                    uint8_t r = (pixel >> 16) & 0xFF;
                    uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                    dst[i] = (rgb565 >> 8) | (rgb565 << 8);
                }
            }
            else
            {
                for (size_t i = 0; i < pixel_count; i++)
                {
                    uint32_t pixel = src[i];
                    uint8_t b = pixel & 0xFF;
                    uint8_t g = (pixel >> 8) & 0xFF;
                    uint8_t r = (pixel >> 16) & 0xFF;
                    dst[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                }
            }
        }
        else
        {
            for (int y = 0; y < effective_height; y++)
            {
                const uint32_t* src_row = src + y * width;
                uint16_t* dst_row = dst + y * display_width;

                for (int x = 0; x < effective_width; x++)
                {
                    uint32_t pixel = src_row[x];
                    uint8_t b = pixel & 0xFF;
                    uint8_t g = (pixel >> 8) & 0xFF;
                    uint8_t r = (pixel >> 16) & 0xFF;
                    uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                    dst_row[x] = swap ? ((rgb565 >> 8) | (rgb565 << 8)) : rgb565;
                }
            }
        }
    }
    else if (format == 0)  // RGB565
    {
        const uint16_t* src = (const uint16_t*)framebuffer;
        uint16_t* dst = conversion_buffer;

        if (present_count == 1)
        {
            DEKI_LOG_DEBUG("  Deki RGB565[0]=0x%04X, RGB565[1]=0x%04X byteswap=%d", src[0], src[1], QuadBlit::GetByteSwap() ? 1 : 0);
        }

        if (QuadBlit::GetByteSwap() && swap)
        {
            // Buffer already in display byte order — just memcpy to DMA buffer
            if (width == display_width && height == display_height)
            {
                memcpy(dst, src, pixel_count * sizeof(uint16_t));
            }
            else
            {
                for (int y = 0; y < effective_height; y++)
                {
                    memcpy(dst + y * display_width, src + y * width, effective_width * sizeof(uint16_t));
                }
            }
        }
        else if (width == display_width && height == display_height)
        {
            for (size_t i = 0; i < pixel_count; i++)
            {
                dst[i] = PrepareRGB565ForDisplay(src[i], swap);
            }
        }
        else
        {
            for (int y = 0; y < effective_height; y++)
            {
                const uint16_t* src_row = src + y * width;
                uint16_t* dst_row = dst + y * display_width;
                for (int x = 0; x < effective_width; x++)
                {
                    dst_row[x] = PrepareRGB565ForDisplay(src_row[x], swap);
                }
            }
        }
    }
    else if (format == 1)  // RGB888
    {
        const uint8_t* src = framebuffer;
        uint16_t* dst = conversion_buffer;

        for (int y = 0; y < effective_height; y++)
        {
            const uint8_t* src_row = src + y * width * 3;
            uint16_t* dst_row = dst + y * display_width;

            for (int x = 0; x < effective_width; x++)
            {
                uint8_t r = src_row[x * 3 + 0];
                uint8_t g = src_row[x * 3 + 1];
                uint8_t b = src_row[x * 3 + 2];
                uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                dst_row[x] = PrepareRGB565ForDisplay(rgb565, swap);
            }
        }
    }
    else
    {
        // Unknown format - fill with black
        memset(conversion_buffer, 0, pixel_count * sizeof(uint16_t));
    }

    } // if (!directBuffer)

    // Composite UI overlay on top if active (ARGB8888 format)
    if (active_overlay && active_overlay->buffer)
    {
        int overlay_width = active_overlay->width < display_width ? active_overlay->width : display_width;
        int overlay_height = active_overlay->height < display_height ? active_overlay->height : display_height;

        const uint32_t* overlay_base = active_overlay->buffer;
        uint16_t* dst_base = conversion_buffer;

        for (int y = 0; y < overlay_height; y++)
        {
            const uint32_t* overlay_row = overlay_base + y * active_overlay->width;
            uint16_t* dst_row = dst_base + y * display_width;

            // Process 4 pixels at a time when possible (unrolled loop for better CPU pipelining)
            int x = 0;
            for (; x + 3 < overlay_width; x += 4)
            {
                uint32_t argb0 = overlay_row[x];
                uint32_t argb1 = overlay_row[x+1];
                uint32_t argb2 = overlay_row[x+2];
                uint32_t argb3 = overlay_row[x+3];

                // Process pixel 0
                if ((argb0 & 0xFF000000) != 0)
                {
                    if ((argb0 & 0xFF000000) == 0xFF000000)
                    {
                        dst_row[x] = PrepareRGB565ForDisplay(
                            ((argb0 >> 8) & 0xF800) | ((argb0 >> 5) & 0x07E0) | ((argb0 >> 3) & 0x001F), swap
                        );
                    }
                    else
                    {
                        uint8_t alpha = argb0 >> 24;
                        uint8_t r = (argb0 >> 16) & 0xFF;
                        uint8_t g = (argb0 >> 8) & 0xFF;
                        uint8_t b = argb0 & 0xFF;

                        uint16_t bg_swapped = dst_row[x];
                        uint16_t bg = swap ? ((bg_swapped >> 8) | (bg_swapped << 8)) : bg_swapped;
                        uint8_t bg_r = (bg >> 8) & 0xF8;
                        uint8_t bg_g = (bg >> 3) & 0xFC;
                        uint8_t bg_b = (bg << 3) & 0xF8;

                        uint8_t inv_alpha = 255 - alpha;
                        uint8_t out_r = (r * alpha + bg_r * inv_alpha + 128) >> 8;
                        uint8_t out_g = (g * alpha + bg_g * inv_alpha + 128) >> 8;
                        uint8_t out_b = (b * alpha + bg_b * inv_alpha + 128) >> 8;

                        dst_row[x] = PrepareRGB565ForDisplay(
                            ((out_r & 0xF8) << 8) | ((out_g & 0xFC) << 3) | (out_b >> 3), swap
                        );
                    }
                }

                // Process pixel 1
                if ((argb1 & 0xFF000000) != 0)
                {
                    if ((argb1 & 0xFF000000) == 0xFF000000)
                    {
                        dst_row[x+1] = PrepareRGB565ForDisplay(
                            ((argb1 >> 8) & 0xF800) | ((argb1 >> 5) & 0x07E0) | ((argb1 >> 3) & 0x001F), swap
                        );
                    }
                    else
                    {
                        uint8_t alpha = argb1 >> 24;
                        uint8_t r = (argb1 >> 16) & 0xFF;
                        uint8_t g = (argb1 >> 8) & 0xFF;
                        uint8_t b = argb1 & 0xFF;

                        uint16_t bg_swapped = dst_row[x+1];
                        uint16_t bg = swap ? ((bg_swapped >> 8) | (bg_swapped << 8)) : bg_swapped;
                        uint8_t bg_r = (bg >> 8) & 0xF8;
                        uint8_t bg_g = (bg >> 3) & 0xFC;
                        uint8_t bg_b = (bg << 3) & 0xF8;

                        uint8_t inv_alpha = 255 - alpha;
                        uint8_t out_r = (r * alpha + bg_r * inv_alpha + 128) >> 8;
                        uint8_t out_g = (g * alpha + bg_g * inv_alpha + 128) >> 8;
                        uint8_t out_b = (b * alpha + bg_b * inv_alpha + 128) >> 8;

                        dst_row[x+1] = PrepareRGB565ForDisplay(
                            ((out_r & 0xF8) << 8) | ((out_g & 0xFC) << 3) | (out_b >> 3), swap
                        );
                    }
                }

                // Process pixel 2
                if ((argb2 & 0xFF000000) != 0)
                {
                    if ((argb2 & 0xFF000000) == 0xFF000000)
                    {
                        dst_row[x+2] = PrepareRGB565ForDisplay(
                            ((argb2 >> 8) & 0xF800) | ((argb2 >> 5) & 0x07E0) | ((argb2 >> 3) & 0x001F), swap
                        );
                    }
                    else
                    {
                        uint8_t alpha = argb2 >> 24;
                        uint8_t r = (argb2 >> 16) & 0xFF;
                        uint8_t g = (argb2 >> 8) & 0xFF;
                        uint8_t b = argb2 & 0xFF;

                        uint16_t bg_swapped = dst_row[x+2];
                        uint16_t bg = swap ? ((bg_swapped >> 8) | (bg_swapped << 8)) : bg_swapped;
                        uint8_t bg_r = (bg >> 8) & 0xF8;
                        uint8_t bg_g = (bg >> 3) & 0xFC;
                        uint8_t bg_b = (bg << 3) & 0xF8;

                        uint8_t inv_alpha = 255 - alpha;
                        uint8_t out_r = (r * alpha + bg_r * inv_alpha + 128) >> 8;
                        uint8_t out_g = (g * alpha + bg_g * inv_alpha + 128) >> 8;
                        uint8_t out_b = (b * alpha + bg_b * inv_alpha + 128) >> 8;

                        dst_row[x+2] = PrepareRGB565ForDisplay(
                            ((out_r & 0xF8) << 8) | ((out_g & 0xFC) << 3) | (out_b >> 3), swap
                        );
                    }
                }

                // Process pixel 3
                if ((argb3 & 0xFF000000) != 0)
                {
                    if ((argb3 & 0xFF000000) == 0xFF000000)
                    {
                        dst_row[x+3] = PrepareRGB565ForDisplay(
                            ((argb3 >> 8) & 0xF800) | ((argb3 >> 5) & 0x07E0) | ((argb3 >> 3) & 0x001F), swap
                        );
                    }
                    else
                    {
                        uint8_t alpha = argb3 >> 24;
                        uint8_t r = (argb3 >> 16) & 0xFF;
                        uint8_t g = (argb3 >> 8) & 0xFF;
                        uint8_t b = argb3 & 0xFF;

                        uint16_t bg_swapped = dst_row[x+3];
                        uint16_t bg = swap ? ((bg_swapped >> 8) | (bg_swapped << 8)) : bg_swapped;
                        uint8_t bg_r = (bg >> 8) & 0xF8;
                        uint8_t bg_g = (bg >> 3) & 0xFC;
                        uint8_t bg_b = (bg << 3) & 0xF8;

                        uint8_t inv_alpha = 255 - alpha;
                        uint8_t out_r = (r * alpha + bg_r * inv_alpha + 128) >> 8;
                        uint8_t out_g = (g * alpha + bg_g * inv_alpha + 128) >> 8;
                        uint8_t out_b = (b * alpha + bg_b * inv_alpha + 128) >> 8;

                        dst_row[x+3] = PrepareRGB565ForDisplay(
                            ((out_r & 0xF8) << 8) | ((out_g & 0xFC) << 3) | (out_b >> 3), swap
                        );
                    }
                }
            }

            // Handle remaining pixels
            for (; x < overlay_width; x++)
            {
                uint32_t argb = overlay_row[x];
                if ((argb & 0xFF000000) == 0) continue;

                if ((argb & 0xFF000000) == 0xFF000000)
                {
                    dst_row[x] = PrepareRGB565ForDisplay(
                        ((argb >> 8) & 0xF800) | ((argb >> 5) & 0x07E0) | ((argb >> 3) & 0x001F), swap
                    );
                }
                else
                {
                    uint8_t alpha = argb >> 24;
                    uint8_t r = (argb >> 16) & 0xFF;
                    uint8_t g = (argb >> 8) & 0xFF;
                    uint8_t b = argb & 0xFF;

                    uint16_t bg_swapped = dst_row[x];
                    uint16_t bg = swap ? ((bg_swapped >> 8) | (bg_swapped << 8)) : bg_swapped;
                    uint8_t bg_r = (bg >> 8) & 0xF8;
                    uint8_t bg_g = (bg >> 3) & 0xFC;
                    uint8_t bg_b = (bg << 3) & 0xF8;

                    uint8_t inv_alpha = 255 - alpha;
                    uint8_t out_r = (r * alpha + bg_r * inv_alpha + 128) >> 8;
                    uint8_t out_g = (g * alpha + bg_g * inv_alpha + 128) >> 8;
                    uint8_t out_b = (b * alpha + bg_b * inv_alpha + 128) >> 8;

                    dst_row[x] = PrepareRGB565ForDisplay(
                        ((out_r & 0xF8) << 8) | ((out_g & 0xFC) << 3) | (out_b >> 3), swap
                    );
                }
            }
        }
    }

    if (m_DoubleBuffer)
    {
        // Standard double-buffer mode: both buffers in PSRAM, swap between them
        if (dma_in_flight)
        {
            tft->waitDMA();
        }

#if defined(ESP32) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        if (m_UsePSRAM)
        {
            size_t buffer_bytes = (buffer_pixel_count * sizeof(uint16_t) + 63) & ~63;
            esp_cache_msync((void*)conversion_buffer, buffer_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        }
#endif

        tft->startWrite();
        tft->pushImage(0, 0, display_width, display_height, conversion_buffer);
        tft->endWrite();
        dma_in_flight = true;

        // Swap to the other buffer for next frame's render
        render_index = 1 - render_index;
    }
    else
    {
        // Single-buffer mode: blocking DMA
        tft->startWrite();
        tft->pushImage(0, 0, display_width, display_height, conversion_buffer);
        tft->endWrite();
        tft->waitDMA();
    }
}

void LovyanGFXDisplay::GetDisplaySize(int32_t* width, int32_t* height) const
{
    if (width) *width = display_width;
    if (height) *height = display_height;
}

bool LovyanGFXDisplay::IsInitialized() const
{
    return initialized;
}

void LovyanGFXDisplay::RequestFullRefresh()
{
    // For LovyanGFX, we always do full refresh, so this is a no-op
}

bool LovyanGFXDisplay::ProcessEvents()
{
    // For embedded platforms, there are no windowing events to process
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

    size_t buffer_size = width * height * sizeof(uint32_t);
    overlay->buffer = (uint32_t*)DekiMemoryProvider::Allocate(buffer_size, true, "UIOverlay-ARGB8888");

    if (!overlay->buffer)
    {
        DEKI_LOG_ERROR("LovyanGFXDisplay::CreateUIOverlay: Failed to allocate overlay buffer (%zu bytes)", buffer_size);
        delete overlay;
        return nullptr;
    }

    memset(overlay->buffer, 0, buffer_size);

    DEKI_LOG_DEBUG("LovyanGFXDisplay: Created UI overlay %dx%d (%zu bytes, ARGB8888 format)", width, height, buffer_size);
    return overlay;
}

bool LovyanGFXDisplay::UpdateUIOverlay(void* overlay, int32_t x, int32_t y,
                                       int32_t width, int32_t height,
                                       const uint32_t* buffer)
{
    if (!overlay || !buffer)
    {
        return false;
    }

    UIOverlay* ui_overlay = (UIOverlay*)overlay;

    if (x < 0 || y < 0 || x + width > ui_overlay->width || y + height > ui_overlay->height)
    {
        DEKI_LOG_WARNING("LovyanGFXDisplay::UpdateUIOverlay: Invalid bounds (%d,%d,%d,%d) for overlay %dx%d",
                         x, y, width, height, ui_overlay->width, ui_overlay->height);
        return false;
    }

    for (int32_t row = 0; row < height; row++)
    {
        uint32_t* dest = &ui_overlay->buffer[(y + row) * ui_overlay->width + x];
        const uint32_t* src = &buffer[row * width];
        memcpy(dest, src, width * sizeof(uint32_t));
    }

    return true;
}

bool LovyanGFXDisplay::UpdateUIOverlayRGB565A8(void* overlay, int32_t x, int32_t y,
                                               int32_t width, int32_t height,
                                               const uint8_t* rgb565a8_pixels)
{
    return false;
}

void LovyanGFXDisplay::DestroyUIOverlay(void* overlay)
{
    if (!overlay)
    {
        return;
    }

    UIOverlay* ui_overlay = (UIOverlay*)overlay;

    if (ui_overlay == active_overlay)
    {
        active_overlay = nullptr;
    }

    if (ui_overlay->buffer)
    {
        DekiMemoryProvider::Free(ui_overlay->buffer, "UIOverlay-ARGB8888");
        ui_overlay->buffer = nullptr;
    }

    delete ui_overlay;

    DEKI_LOG_DEBUG("LovyanGFXDisplay: Destroyed UI overlay");
}

void LovyanGFXDisplay::SetActiveUIOverlay(void* overlay)
{
    active_overlay = (UIOverlay*)overlay;

    if (active_overlay)
    {
        DEKI_LOG_DEBUG("LovyanGFXDisplay: Set active UI overlay %dx%d", active_overlay->width, active_overlay->height);
    }
    else
    {
        DEKI_LOG_DEBUG("LovyanGFXDisplay: Cleared active UI overlay");
    }
}

void LovyanGFXDisplay::ClearActiveUIOverlay()
{
    if (!active_overlay || !active_overlay->buffer)
    {
        return;
    }

    size_t buffer_size = active_overlay->width * active_overlay->height * sizeof(uint32_t);
    memset(active_overlay->buffer, 0, buffer_size);
}

uint8_t* LovyanGFXDisplay::GetRenderBuffer(int32_t* width, int32_t* height)
{
    if (!initialized || !buffers[render_index])
        return nullptr;
    if (width) *width = display_width;
    if (height) *height = display_height;
    return (uint8_t*)buffers[render_index];
}

#else
// Non-ESP32 stub implementation
LovyanGFXDisplay::LovyanGFXDisplay() : tft(nullptr), display_width(0), display_height(0), initialized(false), m_SwapBytes(false),
    buffers{nullptr, nullptr}, buffer_pixel_count(0), render_index(0), dma_in_flight(false),
    m_UsePSRAM(false), m_DoubleBuffer(false), active_overlay(nullptr) {}
LovyanGFXDisplay::~LovyanGFXDisplay() {}
bool LovyanGFXDisplay::InitializeWithDevice(lgfx::LGFX_Device*, int32_t, int32_t, bool, bool, bool) { return false; }
bool LovyanGFXDisplay::Initialize(int32_t width, int32_t height) { return false; }
void LovyanGFXDisplay::Shutdown() {}
void LovyanGFXDisplay::Present(const uint8_t* framebuffer, int width, int height, int format) {}
void LovyanGFXDisplay::ConvertAndRenderFramebuffer(const uint8_t* framebuffer, int width, int height, int format) {}
void LovyanGFXDisplay::GetDisplaySize(int32_t* width, int32_t* height) const {}
bool LovyanGFXDisplay::IsInitialized() const { return false; }
void LovyanGFXDisplay::RequestFullRefresh() {}
bool LovyanGFXDisplay::ProcessEvents() { return true; }
void* LovyanGFXDisplay::CreateUIOverlay(int32_t width, int32_t height) { return nullptr; }
bool LovyanGFXDisplay::UpdateUIOverlay(void* overlay, int32_t x, int32_t y,
                                       int32_t width, int32_t height,
                                       const uint32_t* buffer) { return false; }
bool LovyanGFXDisplay::UpdateUIOverlayRGB565A8(void* overlay, int32_t x, int32_t y,
                                               int32_t width, int32_t height,
                                               const uint8_t* rgb565a8_pixels) { return false; }
void LovyanGFXDisplay::DestroyUIOverlay(void* overlay) {}
void LovyanGFXDisplay::SetActiveUIOverlay(void* overlay) {}
void LovyanGFXDisplay::ClearActiveUIOverlay() {}
uint8_t* LovyanGFXDisplay::GetRenderBuffer(int32_t*, int32_t*) { return nullptr; }
#endif
