#pragma once

#include <cstdint>
#include "SetupComponent.h"
#include "reflection/DekiProperty.h"
#include "LovyanGFXModule.h"

// Forward declaration (must match LovyanGFX's inline namespace)
namespace lgfx { inline namespace v1 { class LGFX_Device; } }

enum class DisplayPanelType : uint8_t
{
    ILI9341 = 0,
    ST7789 = 1,
    ST7735 = 2,
    GC9A01 = 3,
    SSD1351 = 4
};

enum class DisplayBusType : uint8_t
{
    SPI = 0,
    Parallel8bit = 1,
    Parallel16bit = 2
};

/**
 * @brief Component to configure and initialize a LovyanGFX display at runtime
 *
 * Add this component to your boot prefab to set up display hardware.
 * Configure the display panel type, bus type, and pin mappings in the Inspector.
 * Replaces the compile-time LGFX_Config.h approach with runtime configuration.
 *
 * Inherits from SetupComponent to participate in boot sequence.
 * PlatformSetupComponent calls Setup() to initialize the display.
 * Must run BEFORE LGFXTouchPanel in the boot sequence.
 *
 * Usage:
 * 1. Add LGFXDisplayPanel to your boot prefab
 * 2. Configure panel type, bus, and pins in Inspector
 * 3. Add to PlatformSetupComponent's setup_components list (before touch)
 */
class DEKI_LOVYANGFX_API LGFXDisplayPanel : public SetupComponent
{
public:
    DEKI_COMPONENT(LGFXDisplayPanel, SetupComponent, "LovyanGFX", "a7e1d3f0-8b4c-4e2a-9f61-3c8d2b5a7e90", "DEKI_FEATURE_LGFX_DISPLAY_PANEL")

    // ========== Panel ==========

    DEKI_EXPORT
    DEKI_TOOLTIP("Display panel driver IC")
    DisplayPanelType panelType = DisplayPanelType::ILI9341;

    DEKI_EXPORT
    DEKI_TOOLTIP("Display width in pixels")
    DEKI_RANGE(1, 1024)
    int32_t panel_width = 320;

    DEKI_EXPORT
    DEKI_TOOLTIP("Display height in pixels")
    DEKI_RANGE(1, 1024)
    int32_t panel_height = 240;

    DEKI_EXPORT
    DEKI_TOOLTIP("Display rotation (0-3)")
    DEKI_RANGE(0, 3)
    int32_t rotation = 1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Pixel offset X (for panels with non-zero origin)")
    int32_t offset_x = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("Pixel offset Y (for panels with non-zero origin)")
    int32_t offset_y = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("Invert display colors")
    bool invert_color = false;

    DEKI_EXPORT
    DEKI_TOOLTIP("Swap R and B color channels (RGB vs BGR)")
    bool rgb_order = false;

    // ========== Bus ==========

    DEKI_GROUP("Bus")
    DEKI_EXPORT
    DEKI_TOOLTIP("Display bus type")
    DisplayBusType busType = DisplayBusType::Parallel8bit;

    // --- SPI Bus Pins ---

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI MOSI pin")
    DEKI_VISIBLE_WHEN(busType, SPI)
    DEKI_RANGE(-1, 48)
    int32_t spi_mosi = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI MISO pin (-1 = not used)")
    DEKI_VISIBLE_WHEN(busType, SPI)
    DEKI_RANGE(-1, 48)
    int32_t spi_miso = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI clock pin")
    DEKI_VISIBLE_WHEN(busType, SPI)
    DEKI_RANGE(-1, 48)
    int32_t spi_clk = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI data/command pin")
    DEKI_VISIBLE_WHEN(busType, SPI)
    DEKI_RANGE(-1, 48)
    int32_t spi_dc = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI host (0=VSPI, 1=HSPI)")
    DEKI_VISIBLE_WHEN(busType, SPI)
    DEKI_RANGE(0, 1)
    int32_t spi_host = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI write frequency in Hz")
    DEKI_VISIBLE_WHEN(busType, SPI)
    int32_t spi_freq_write = 40000000;

    // --- Parallel Bus Pins (8-bit and 16-bit) ---

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel bus write frequency in Hz")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    int32_t par_freq_write = 40000000;

    DEKI_EXPORT
    DEKI_TOOLTIP("Register select / data-command pin (RS/DC)")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t rs_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel bus write strobe pin")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t wr_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel bus read strobe pin (-1 = not used)")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t rd_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D0")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d0_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D1")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d1_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D2")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d2_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D3")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d3_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D4")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d4_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D5")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d5_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D6")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d6_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D7")
    DEKI_VISIBLE_WHEN(busType, Parallel8bit, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d7_pin = -1;

    // --- 16-bit only data pins ---

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D8")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d8_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D9")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d9_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D10")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d10_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D11")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d11_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D12")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d12_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D13")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d13_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D14")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d14_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Parallel data pin D15")
    DEKI_VISIBLE_WHEN(busType, Parallel16bit)
    DEKI_RANGE(-1, 48)
    int32_t d15_pin = -1;

    // ========== Control Pins ==========

    DEKI_GROUP("Control Pins")
    DEKI_EXPORT
    DEKI_TOOLTIP("Chip select pin (-1 = not used)")
    DEKI_RANGE(-1, 48)
    int32_t pin_cs = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Reset pin (-1 = not connected)")
    DEKI_RANGE(-1, 48)
    int32_t pin_rst = -1;

    // ========== Backlight ==========

    DEKI_GROUP("Backlight")
    DEKI_EXPORT
    DEKI_TOOLTIP("Backlight pin (-1 = none)")
    DEKI_RANGE(-1, 48)
    int32_t bl_pin = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("PWM channel for backlight")
    DEKI_RANGE(0, 15)
    int32_t bl_pwm_channel = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("Invert backlight signal (active low)")
    bool bl_invert = false;

    // ========== Advanced ==========

    DEKI_GROUP("Advanced")
    DEKI_EXPORT
    DEKI_TOOLTIP("Swap RGB565 byte order for display transfer")
    bool swap_bytes = false;

    DEKI_EXPORT
    DEKI_TOOLTIP("Panel memory width (may differ from visible width)")
    DEKI_RANGE(1, 1024)
    int32_t memory_width = 320;

    DEKI_EXPORT
    DEKI_TOOLTIP("Panel memory height (may differ from visible height)")
    DEKI_RANGE(1, 1024)
    int32_t memory_height = 240;

    DEKI_EXPORT
    DEKI_TOOLTIP("Allocate display buffers in PSRAM instead of internal RAM")
    bool use_psram = false;

    DEKI_EXPORT
    DEKI_TOOLTIP("Use double buffering for async DMA (overlaps render and display transfer)")
    bool double_buffer = false;

    // ========== SetupComponent Implementation ==========

    void Setup(SetupCallback onComplete) override;
    const char* GetSetupName() const override { return "Display Panel"; }

    // ========== Static Accessor ==========

    /**
     * @brief Get the initialized LGFX device instance
     * @return Pointer to device, or nullptr if not yet initialized
     */
    static lgfx::LGFX_Device* GetLGFXDevice();
};

// Generated property metadata
#include "generated/LGFXDisplayPanel.gen.h"
