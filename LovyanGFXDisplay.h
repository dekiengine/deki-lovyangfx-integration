#pragma once

#include <cstddef>  // size_t

#include <deki/Engine.h>  // ColorFormat
#include <deki/providers/IDisplay.h>

#include <vector>

// Forward declaration; must match LovyanGFX's inline namespace. Must stay at
// global scope: LovyanGFX declares ::lgfx, so declaring it inside
// DekiLovyanGfx would create a separate DekiLovyanGfx::lgfx that is never
// defined.
namespace lgfx
{
inline namespace v1
{
class LGFX_Device;
}
}  // namespace lgfx

namespace DekiLovyanGfx
{

/// Deki::IDisplay on LovyanGFX: wraps an lgfx::LGFX_Device that the
/// LGFXDisplayPanel component has created and configured.
class LovyanGFXDisplay : public Deki::IDisplay
{
private:
    lgfx::LGFX_Device* tft;
    int32_t m_DisplayWidth;
    int32_t m_DisplayHeight;
    bool initialized;

    // Double buffering for async DMA
    uint16_t* buffers[2];  // [0] = primary, [1] = secondary (null if single-buffer)
    size_t m_BufferPixelCount;
    int m_RenderIndex;  // index of the buffer being rendered to
    bool m_DmaInFlight;
    bool m_UsePSRAM;
    bool m_DoubleBuffer;
    bool m_SwapBytes;

    // UI overlay
    struct UIOverlay
    {
        uint32_t* buffer;  // ARGB8888 framebuffer
        int32_t width;
        int32_t height;
    };
    UIOverlay* m_ActiveOverlay;

    // Partial present: rows are pushed through two small DMA-capable staging
    // bands, where they are converted to RGB565 and byte-swapped for the panel,
    // so the engine's framebuffer is never changed. Rows per band is a policy:
    // larger bands mean fewer pushes and cost width * rows * 4 bytes of
    // internal RAM for the pair.
    static constexpr int kBandRows = 8;
    // Bands start and end on a multiple of this many rows. 1 for most panels;
    // the RM67162 ignores a write whose start or size is odd along its short
    // axis, which in landscape is the rows. Must divide kBandRows.
    int m_RowAlign = 1;
    uint16_t* m_Band[2] = { nullptr, nullptr };
    int m_BandIndex = 0;
    std::vector<Deki::Rect> m_BandScratch;

public:
    LovyanGFXDisplay();
    virtual ~LovyanGFXDisplay();

    /// Starts with an LGFX device LGFXDisplayPanel has created and configured.
    bool InitializeWithDevice(lgfx::LGFX_Device* device, int32_t width, int32_t height, bool swapBytes = false,
                              bool usePSRAM = false, bool doubleBuffer = false);

    /// For a panel that only accepts writes aligned to `rows` (1, 2, 4 or 8).
    void SetRowAlignment(int rows) { m_RowAlign = (rows == 2 || rows == 4 || rows == 8) ? rows : 1; }

    // IPlatformDisplay interface
    bool Initialize(int32_t width, int32_t height) override;
    void Shutdown() override;
    void Present(const uint8_t* framebuffer, int width, int height, Deki::ColorFormat format) override;
    bool SupportsPartialPresent() const override;
    void PresentRegions(const uint8_t* framebuffer, int width, int height, Deki::ColorFormat format,
                        const Deki::Rect* rects, int32_t count) override;
    void GetDisplaySize(int32_t* width, int32_t* height) const override;
    // Every panel LovyanGFX drives here takes RGB565, byte-swapped on the wire when it asks.
    Deki::ColorFormat GetColorFormat() const override { return Deki::ColorFormat::RGB565; }
    bool IsInitialized() const override;
    void RequestFullRefresh() override;
    bool ProcessEvents() override;

    // UI overlay (required by IPlatformDisplay)
    void* CreateUIOverlay(int32_t width, int32_t height) override;
    bool UpdateUIOverlay(void* overlay, int32_t x, int32_t y, int32_t width, int32_t height,
                         const uint32_t* buffer) override;
    bool UpdateUIOverlayRGB565A8(void* overlay, int32_t x, int32_t y, int32_t width, int32_t height,
                                 const uint8_t* rgb565a8Pixels) override;
    void DestroyUIOverlay(void* overlay) override;
    void SetActiveUIOverlay(void* overlay) override;
    void ClearActiveUIOverlay() override;
    uint8_t* GetRenderBuffer(int32_t* width, int32_t* height) override;
    void SetBacklight(bool on) override;

    // LovyanGFX-specific
    lgfx::LGFX_Device* GetTFT() const { return tft; }

private:
    void ConvertAndRenderFramebuffer(const uint8_t* framebuffer, int width, int height, Deki::ColorFormat format);
    bool EnsureBands();
    void FreeBands();
    // Converts rows [y0, y1) of the framebuffer into staging bands and pushes them.
    void PushRows(const uint8_t* framebuffer, int width, int height, Deki::ColorFormat format, int y0, int y1);
    // Flips the render buffer (double buffering) or waits for DMA (single).
    void FinishPresent();
};

}  // namespace DekiLovyanGfx
