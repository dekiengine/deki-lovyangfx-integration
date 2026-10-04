#pragma once

#include <cstdint>
#include <deki/SetupComponent.h>
#include <deki/reflection/Property.h>
#include "LovyanGFXPackage.h"

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

// RM67162 is an AMOLED controller that only speaks QSPI.
enum class DisplayPanelType : uint8_t
{
    ILI9341 = 0,
    ST7789 = 1,
    ST7735 = 2,
    GC9A01 = 3,
    SSD1351 = 4,
    ST7789P3 = 5,
    RM67162 = 6
};

enum class DisplayBusType : uint8_t
{
    SPI = 0,
    Parallel8bit = 1,
    Parallel16bit = 2,
    QSPI = 3
};

enum class DisplayRotation : uint8_t
{
    Portrait = 0,
    Landscape = 1,
    Portrait180 = 2,
    Landscape270 = 3,
    PortraitMirror = 4,
    LandscapeMirror = 5,
    Portrait180Mirror = 6,
    Landscape270Mirror = 7
};

/// Configures and starts a LovyanGFX display at runtime, from the boot scene.
/// The panel type, bus and pins are set in the Inspector, with no compile-time
/// LGFX_Config.h.
///
/// As a SetupComponent it is part of the boot sequence: PlatformSetupComponent
/// calls Setup() to start the display. It must run before LGFXTouchPanel.
///
/// Usage:
/// 1. Add LGFXDisplayPanel to your boot scene
/// 2. Set the panel type, bus and pins in the Inspector
/// 3. Add it to PlatformSetupComponent's setup_components list, before touch
DEKI_CATEGORY("LovyanGFX")
DEKI_DESCRIPTION("Drives a display panel (ILI9341, ST7789, GC9A01, RM67162 AMOLED, ...) over SPI, QSPI or a parallel "
                 "bus through LovyanGFX.")
DEKI_FORMER_NAME("LGFXDisplayPanel")
class DEKI_LOVYANGFX_API LGFXDisplayPanel : public Deki::SetupComponent
{
public:
    // ========== Panel ==========

    DEKI_EXPORT
    DEKI_TOOLTIP("Display panel driver IC")
    DisplayPanelType panelType = DisplayPanelType::ILI9341;

    DEKI_EXPORT
    DEKI_TOOLTIP("Display width in pixels")
    DEKI_RANGE(1, 1024)
    int32_t panelWidth = 320;

    DEKI_EXPORT
    DEKI_TOOLTIP("Display height in pixels")
    DEKI_RANGE(1, 1024)
    int32_t panelHeight = 240;

    DEKI_EXPORT
    DEKI_TOOLTIP("Display rotation")
    DisplayRotation rotation = DisplayRotation::Landscape;

    DEKI_EXPORT
    DEKI_TOOLTIP("Pixel offset X (for panels with non-zero origin)")
    int32_t offsetX = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("Pixel offset Y (for panels with non-zero origin)")
    int32_t offsetY = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("Invert display colors")
    bool invertColor = false;

    DEKI_EXPORT
    DEKI_TOOLTIP("Swap R and B color channels (RGB vs BGR)")
    bool rgbOrder = false;

    // ========== Bus ==========

    DEKI_GROUP("Bus")
    DEKI_EXPORT
    DEKI_TOOLTIP("Display bus type")
    DisplayBusType busType = DisplayBusType::Parallel8bit;

    // --- SPI bus pins ---

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI MOSI pin")
    DEKI_VISIBLE_WHEN(busType, SPI)
    DEKI_RANGE(-1, 48)
    int32_t mosiPin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI MISO pin (-1 = not used)")
    DEKI_VISIBLE_WHEN(busType, SPI)
    DEKI_RANGE(-1, 48)
    int32_t misoPin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI clock pin")
    DEKI_VISIBLE_WHEN(busType, SPI, QSPI)
    DEKI_RANGE(-1, 48)
    int32_t clkPin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI data/command pin")
    DEKI_VISIBLE_WHEN(busType, SPI)
    DEKI_RANGE(-1, 48)
    int32_t dcPin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI host, as ESP-IDF numbers them (on an ESP32-S3: 1 = SPI2, 2 = SPI3)")
    DEKI_VISIBLE_WHEN(busType, SPI, QSPI)
    DEKI_RANGE(0, 2)
    int32_t spiPort = 0;

    DEKI_EXPORT
    DEKI_UNIT(Frequency)
    DEKI_TOOLTIP("SPI write frequency in Hz")
    DEKI_VISIBLE_WHEN(busType, SPI, QSPI)
    int32_t spiWriteHz = 40000000;

    // --- QSPI bus pins (clock, host and frequency are the SPI ones above) ---

    DEKI_EXPORT
    DEKI_TOOLTIP("QSPI data pin IO0")
    DEKI_VISIBLE_WHEN(busType, QSPI)
    DEKI_RANGE(-1, 48)
    int32_t io0Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("QSPI data pin IO1")
    DEKI_VISIBLE_WHEN(busType, QSPI)
    DEKI_RANGE(-1, 48)
    int32_t io1Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("QSPI data pin IO2")
    DEKI_VISIBLE_WHEN(busType, QSPI)
    DEKI_RANGE(-1, 48)
    int32_t io2Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("QSPI data pin IO3")
    DEKI_VISIBLE_WHEN(busType, QSPI)
    DEKI_RANGE(-1, 48)
    int32_t io3Pin = -1;

    // --- Parallel bus pins (8-bit and 16-bit) ---

    DEKI_EXPORT
    DEKI_UNIT(Frequency)
    DEKI_TOOLTIP("Parallel bus write frequency in Hz")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    int32_t parWriteHz = 40000000;

    DEKI_EXPORT
    DEKI_TOOLTIP("Register select / data-command pin (RS/DC)")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t rsPin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel bus write strobe pin")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t wrPin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel bus read strobe pin (-1 = not used)")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t rdPin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D0")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d0Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D1")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d1Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D2")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d2Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D3")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d3Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D4")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d4Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D5")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d5Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D6")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d6Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D7")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d7Pin = -1;

    // --- 16-bit only data pins ---

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D8")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d8Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D9")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d9Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D10")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d10Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D11")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d11Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D12")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d12Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D13")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d13Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D14")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d14Pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D15")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d15Pin = -1;

    // ========== Control pins ==========

    DEKI_GROUP("Control Pins")
    DEKI_EXPORT
    DEKI_TOOLTIP("Chip select pin (-1 = not used)")
    DEKI_RANGE(-1, 48)
    int32_t csPin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Reset pin (-1 = not connected)")
    DEKI_RANGE(-1, 48)
    int32_t rstPin = -1;

    // ========== Backlight ==========

    DEKI_GROUP("Backlight")
    DEKI_EXPORT
    DEKI_TOOLTIP("Backlight pin (-1 = none)")
    DEKI_RANGE(-1, 48)
    int32_t blPin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("PWM channel for backlight")
    DEKI_RANGE(0, 15)
    int32_t blPwmChannel = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("Invert backlight signal (active low)")
    bool blInvert = false;

    // ========== Advanced ==========

    DEKI_GROUP("Advanced")
    DEKI_EXPORT
    DEKI_TOOLTIP("Swap RGB565 byte order for display transfer")
    bool swapBytes = false;

    DEKI_EXPORT
    DEKI_TOOLTIP("Panel memory width (may differ from visible width)")
    DEKI_RANGE(1, 1024)
    int32_t memoryWidth = 320;

    DEKI_EXPORT
    DEKI_TOOLTIP("Panel memory height (may differ from visible height)")
    DEKI_RANGE(1, 1024)
    int32_t memoryHeight = 240;

    DEKI_EXPORT
    DEKI_TOOLTIP("Keep the framebuffer in PSRAM instead of internal RAM: frees a screen's worth of internal RAM, "
                 "drawing is slower")
    bool usePsram = false;

    DEKI_EXPORT
    DEKI_TOOLTIP("Use double buffering for async DMA (overlaps render and display transfer)")
    bool doubleBuffer = false;

    // ========== SetupComponent ==========

    void Setup(SetupCallback onComplete) override;
    const char* GetSetupName() const override { return "Display Panel"; }

    /// The started LGFX device, or nullptr before Setup() has run.
    static lgfx::LGFX_Device* GetLGFXDevice();
};

}  // namespace DekiLovyanGfx
