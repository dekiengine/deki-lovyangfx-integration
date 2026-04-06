#pragma once

#include <cstddef>  // for size_t

#include "providers/IDekiDisplay.h"

// Forward declaration (must match LovyanGFX's inline namespace)
namespace lgfx { inline namespace v1 { class LGFX_Device; } }

/**
 * @brief LovyanGFX implementation of display interface
 *
 * Wraps a pre-configured lgfx::LGFX_Device to implement IDekiDisplay.
 * The LGFX device is created and configured by LGFXDisplayPanel component.
 */
class LovyanGFXDisplay : public IDekiDisplay
{
   private:
    lgfx::LGFX_Device* tft;
    int32_t display_width;
    int32_t display_height;
    bool initialized;

    // Double-buffer support for async DMA
    uint16_t* buffers[2];        // [0] = primary, [1] = secondary (null if single-buffer)
    size_t buffer_pixel_count;
    int render_index;            // Index of buffer currently being rendered to
    bool dma_in_flight;
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
    UIOverlay* active_overlay;

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

    // LovyanGFX-specific methods
    lgfx::LGFX_Device* GetTFT() const { return tft; }

   private:
    void ConvertAndRenderFramebuffer(const uint8_t* framebuffer, int width, int height, int format);
};
