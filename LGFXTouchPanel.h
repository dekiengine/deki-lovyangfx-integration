#pragma once

#include <cstdint>
#include "SetupComponent.h"
#include "reflection/DekiProperty.h"
#include "LovyanGFXModule.h"

enum class TouchDriverType : uint8_t
{
    FT5x06 = 0,
    GT911 = 1,
    CST816S = 2,
    XPT2046 = 3
};

enum class TouchRotation : uint8_t
{
    None = 0,
    CW_90 = 1,
    CW_180 = 2,
    CW_270 = 3,
    Mirror = 4,
    Mirror_CW_90 = 5,
    Mirror_CW_180 = 6,
    Mirror_CW_270 = 7
};

/**
 * @brief Component to configure and initialize a touch panel at runtime
 *
 * Add this component to your boot prefab to enable touch input.
 * Supports multiple LovyanGFX touch drivers: FT5x06, GT911, XPT2046, CST816S.
 *
 * Inherits from SetupComponent to participate in boot sequence.
 * PlatformSetupComponent calls Setup() to initialize the touch controller
 * and attach it to the LovyanGFX display panel.
 *
 * Usage:
 * 1. Add LGFXTouchPanel to your boot prefab
 * 2. Set driver type and pin values in Inspector
 * 3. Add to PlatformSetupComponent's setup_components list
 */
class DEKI_LOVYANGFX_API LGFXTouchPanel : public SetupComponent
{
public:
    DEKI_COMPONENT(LGFXTouchPanel, SetupComponent, "LovyanGFX", "9c79bf8d-6ba3-48b8-80da-1e775d818ad3", "DEKI_FEATURE_LGFX_TOUCH_PANEL")

    // ========== Driver Selection ==========

    DEKI_EXPORT
    DEKI_TOOLTIP("Touch controller IC")
    TouchDriverType driverType = TouchDriverType::FT5x06;

    // ========== Pins ==========

    DEKI_GROUP("Pins")
    DEKI_EXPORT
    DEKI_TOOLTIP("I2C data pin for capacitive touch")
    DEKI_VISIBLE_WHEN(driverType, FT5x06, GT911, CST816S)
    DEKI_RANGE(-1, 48)
    int32_t sda_pin = 21;

    DEKI_EXPORT
    DEKI_TOOLTIP("I2C clock pin for capacitive touch")
    DEKI_VISIBLE_WHEN(driverType, FT5x06, GT911, CST816S)
    DEKI_RANGE(-1, 48)
    int32_t scl_pin = 20;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI chip select pin (-1 = not used)")
    DEKI_VISIBLE_WHEN(driverType, XPT2046)
    DEKI_RANGE(-1, 48)
    int32_t spi_cs = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI MOSI pin (-1 = not used)")
    DEKI_VISIBLE_WHEN(driverType, XPT2046)
    DEKI_RANGE(-1, 48)
    int32_t spi_mosi = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI MISO pin (-1 = not used)")
    DEKI_VISIBLE_WHEN(driverType, XPT2046)
    DEKI_RANGE(-1, 48)
    int32_t spi_miso = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("SPI clock pin (-1 = not used)")
    DEKI_VISIBLE_WHEN(driverType, XPT2046)
    DEKI_RANGE(-1, 48)
    int32_t spi_clk = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Interrupt pin for touch events (-1 = polling mode)")
    DEKI_RANGE(-1, 48)
    int32_t pin_int = -1;

    DEKI_EXPORT
    DEKI_TOOLTIP("Reset pin for touch controller (-1 = not connected)")
    DEKI_RANGE(-1, 48)
    int32_t pin_rst = -1;

    // ========== Touch Panel Bounds ==========

    DEKI_GROUP("Touch Panel Bounds")
    DEKI_EXPORT
    DEKI_TOOLTIP("Minimum raw X value from touch controller")
    int32_t x_min = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("Maximum raw X value from touch controller")
    int32_t x_max = 239;

    DEKI_EXPORT
    DEKI_TOOLTIP("Minimum raw Y value from touch controller")
    int32_t y_min = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("Maximum raw Y value from touch controller")
    int32_t y_max = 319;

    // ========== Advanced ==========

    DEKI_GROUP("Advanced")
    DEKI_EXPORT
    DEKI_TOOLTIP("Touch coordinate rotation, must match display rotation")
    TouchRotation offset_rotation = TouchRotation::None;

    DEKI_EXPORT
    DEKI_TOOLTIP("Enable if touch shares the SPI bus with the display")
    bool bus_shared = false;

    DEKI_EXPORT
    DEKI_TOOLTIP("Override default I2C settings (address, port, frequency)")
    DEKI_VISIBLE_WHEN(driverType, FT5x06, GT911, CST816S)
    bool i2c_override = false;

    DEKI_EXPORT
    DEKI_TOOLTIP("I2C device address (e.g. 0x38 for FT5x06, 0x5D for GT911)")
    DEKI_VISIBLE_WHEN(i2c_override, 1)
    int32_t i2c_addr = 0x38;

    DEKI_EXPORT
    DEKI_TOOLTIP("I2C peripheral port number (0 or 1)")
    DEKI_VISIBLE_WHEN(i2c_override, 1)
    int32_t i2c_port = 0;

    DEKI_EXPORT
    DEKI_TOOLTIP("I2C bus frequency in Hz (default 100000)")
    DEKI_VISIBLE_WHEN(i2c_override, 1)
    int32_t i2c_freq = 100000;

    // ========== SetupComponent Implementation ==========

    void Setup(SetupCallback onComplete) override;
    const char* GetSetupName() const override { return "Touch Panel"; }
};

// Generated property metadata
#include "generated/LGFXTouchPanel.gen.h"
