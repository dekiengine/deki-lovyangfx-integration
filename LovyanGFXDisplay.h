#pragma once

#include <cstddef>  // for size_t

#include "providers/IDekiDisplay.h"

// Forward declaration (must match LovyanGFX's inline namespace)
#include <vector>

namespace lgfx { inline namespace v1 { class LGFX_Device; } }

/**
 * @brief LovyanGFX implementation of display interface
 *
 * Wraps a pre-configured lgfx::LGFX_Device to implement Deki::IDisplay.
 * The LGFX device is created and configured by LGFXDisplayPanel component.
 */
class LovyanGFXDisplay : public Deki::IDisplay
{
   private:
    lgfx::LGFX_Device* tft;
    int32_t m_DisplayWidth;
    int32_t m_DisplayHeight;
    bool initialized;

    // Double-buffer support for async DMA
    uint16_t* buffers[2];        // [0] = primary, [1] = secondary (null if single-buffer)
    size_t m_BufferPixelCount;
    int m_RenderIndex;            // Index of buffer currently being rendered to
    bool m_DmaInFlight;
    bool m_UsePSRAM;
    bool m_DoubleBuffer;
    bool m_SwapBytes;

    // UI overlay support
    struct UIOverlay
    {
        uint32_t* buffer;  // ARGB8888 framebuffer
        int32_t width;
        int32_t height;
    };
    UIOverlay* m_ActiveOverlay;

    // Partial present: rows are pushed through two small DMA-capable staging
    // bands (converted to RGB565 and byte-swapped for the panel there, so the
    // engine's framebuffer is never mutated). Rows per band is a policy:
    // larger bands mean fewer pushes and width * rows * 4 bytes of internal
    // RAM for the pair.
    static constexpr int kBandRows = 8;
    uint16_t* m_Band[2] = { nullptr, nullptr };
    int m_BandIndex = 0;
    std::vector<Deki::Rect> m_BandScratch;

   public:
    LovyanGFXDisplay();
    virtual ~LovyanGFXDisplay();

    // Initialize with a pre-configured LGFX device (created by LGFXDisplayPanel)
    bool InitializeWithDevice(lgfx::LGFX_Device* device, int32_t width, int32_t height,
                              bool swapBytes = false, bool usePSRAM = false, bool doubleBuffer = false);

    // IPlatformDisplay interface
    bool Initialize(int32_t width, int32_t height) override;
    void Shutdown() override;
    void Present(const uint8_t* framebuffer, int width, int height, int format) override;
    bool SupportsPartialPresent() const override;
    void PresentRegions(const uint8_t* framebuffer, int width, int height, int format,
                        const Deki::Rect* rects, int32_t count) override;
    void GetDisplaySize(int32_t* width, int32_t* height) const override;
    bool IsInitialized() const override;
    void RequestFullRefresh() override;
    bool ProcessEvents() override;

    // UI Overlay methods (required by IPlatformDisplay)
    void* CreateUIOverlay(int32_t width, int32_t height) override;
    bool UpdateUIOverlay(void* overlay, int32_t x, int32_t y,
                        int32_t width, int32_t height,
                        const uint32_t* buffer) override;
    bool UpdateUIOverlayRGB565A8(void* overlay, int32_t x, int32_t y,
                                 int32_t width, int32_t height,
                                 const uint8_t* rgb565a8_pixels) override;
    void DestroyUIOverlay(void* overlay) override;
    void SetActiveUIOverlay(void* overlay) override;
    void ClearActiveUIOverlay() override;
    uint8_t* GetRenderBuffer(int32_t* width, int32_t* height) override;
    void SetBacklight(bool on) override;

    // LovyanGFX-specific methods
    lgfx::LGFX_Device* GetTFT() const { return tft; }

   private:
    void ConvertAndRenderFramebuffer(const uint8_t* framebuffer, int width, int height, int format);
    bool EnsureBands();
    void FreeBands();
    // Convert rows [y0, y1) of the framebuffer into staging bands and push them.
    void PushRows(const uint8_t* framebuffer, int width, int height, int format, int y0, int y1);
    // Flip the render buffer (double buffering) / wait for DMA (single).
    void FinishPresent();
};
