#include "LGFXTouchPanel.h"
#include "DekiLogSystem.h"
#include "DekiEngine.h"
#include "PrefabSystem.h"
#include "providers/DekiInputProvider.h"
#include "providers/DekiI2CProvider.h"
#include "providers/IDekiI2C.h"

#if defined(ESP32)

#include <LovyanGFX.hpp>
#include "LGFXDisplayPanel.h"
#include "LovyanGFXTouch.h"
#include "esp_log.h"
static const char* TAG = "LGFXTouch";

void LGFXTouchPanel::Setup(SetupCallback onComplete)
{
    ESP_LOGI(TAG, "Setting up touch (driver=%d)", static_cast<int>(driverType));
    DEKI_LOG_INFO("LGFXTouchPanel: Setting up touch panel (driver=%d)", static_cast<int>(driverType));

    lgfx::LGFX_Device* gfxDevice = LGFXDisplayPanel::GetLGFXDevice();
    if (!gfxDevice)
    {
        DEKI_LOG_ERROR("LGFXTouchPanel: No LGFX device available (LGFXDisplayPanel not setup yet?)");
        onComplete(false);
        return;
    }

    // Resolve shared I2C bus (only for capacitive drivers; XPT2046 is SPI).
    int      bus_sda  = -1;
    int      bus_scl  = -1;
    int      bus_freq = 0;
    const bool is_i2c_driver =
        driverType == TouchDriverType::FT5x06 ||
        driverType == TouchDriverType::GT911  ||
        driverType == TouchDriverType::CST816S;

    if (is_i2c_driver)
    {
        IDekiI2C* bus = DekiI2CProvider::GetBus(i2c_port);
        if (!bus)
        {
            DEKI_LOG_ERROR("LGFXTouchPanel: no I2C bus on port %d — add I2CBusComponent before LGFXTouchPanel in boot prefab", (int)i2c_port);
            onComplete(false);
            return;
        }
        bus_sda  = bus->GetSdaPin();
        bus_scl  = bus->GetSclPin();
        bus_freq = bus->GetFrequencyHz();
    }

    lgfx::ITouch* touch = nullptr;

    // Create the appropriate touch driver
    switch (driverType)
    {
        case TouchDriverType::FT5x06:
        {
            auto* t = new lgfx::Touch_FT5x06();
            auto cfg = t->config();
            cfg.x_min = x_min;
            cfg.x_max = x_max;
            cfg.y_min = y_min;
            cfg.y_max = y_max;
            cfg.pin_int = pin_int;
            cfg.pin_rst = pin_rst;
            cfg.pin_sda = bus_sda;
            cfg.pin_scl = bus_scl;
            cfg.i2c_addr = 0x38;
            cfg.i2c_port = i2c_port;
            cfg.freq = bus_freq;
            cfg.bus_shared = true;
            cfg.offset_rotation = static_cast<uint8_t>(offset_rotation);
            t->config(cfg);
            touch = t;
            DEKI_LOG_INFO("LGFXTouchPanel: FT5x06 on I2C port %d (SDA=%d SCL=%d freq=%d)", (int)i2c_port, bus_sda, bus_scl, bus_freq);
            break;
        }
        case TouchDriverType::GT911:
        {
            auto* t = new lgfx::Touch_GT911();
            auto cfg = t->config();
            cfg.x_min = x_min;
            cfg.x_max = x_max;
            cfg.y_min = y_min;
            cfg.y_max = y_max;
            cfg.pin_int = pin_int;
            cfg.pin_rst = pin_rst;
            cfg.pin_sda = bus_sda;
            cfg.pin_scl = bus_scl;
            cfg.i2c_addr = 0x5D;
            cfg.i2c_port = i2c_port;
            cfg.freq = bus_freq;
            cfg.bus_shared = true;
            cfg.offset_rotation = static_cast<uint8_t>(offset_rotation);
            t->config(cfg);
            touch = t;
            DEKI_LOG_INFO("LGFXTouchPanel: GT911 on I2C port %d (SDA=%d SCL=%d freq=%d)", (int)i2c_port, bus_sda, bus_scl, bus_freq);
            break;
        }
        case TouchDriverType::CST816S:
        {
            auto* t = new lgfx::Touch_CST816S();
            auto cfg = t->config();
            cfg.x_min = x_min;
            cfg.x_max = x_max;
            cfg.y_min = y_min;
            cfg.y_max = y_max;
            cfg.pin_int = pin_int;
            cfg.pin_rst = pin_rst;
            cfg.pin_sda = bus_sda;
            cfg.pin_scl = bus_scl;
            cfg.i2c_addr = 0x15;
            cfg.i2c_port = i2c_port;
            cfg.freq = bus_freq;
            cfg.bus_shared = true;
            cfg.offset_rotation = static_cast<uint8_t>(offset_rotation);
            t->config(cfg);
            touch = t;
            DEKI_LOG_INFO("LGFXTouchPanel: CST816S on I2C port %d (SDA=%d SCL=%d freq=%d)", (int)i2c_port, bus_sda, bus_scl, bus_freq);
            break;
        }
        case TouchDriverType::XPT2046:
        {
            auto* t = new lgfx::Touch_XPT2046();
            auto cfg = t->config();
            cfg.x_min = x_min;
            cfg.x_max = x_max;
            cfg.y_min = y_min;
            cfg.y_max = y_max;
            cfg.pin_int = pin_int;
            cfg.pin_rst = pin_rst;
            cfg.pin_cs = spi_cs;
            cfg.pin_mosi = spi_mosi;
            cfg.pin_miso = spi_miso;
            cfg.pin_sclk = spi_clk;
            cfg.bus_shared = bus_shared;
            cfg.offset_rotation = static_cast<uint8_t>(offset_rotation);
            t->config(cfg);
            touch = t;
            DEKI_LOG_INFO("LGFXTouchPanel: Using XPT2046 driver (SPI)");
            break;
        }
        default:
            DEKI_LOG_ERROR("LGFXTouchPanel: Unknown driver type %d", static_cast<int>(driverType));
            onComplete(false);
            return;
    }

    // Attach touch controller to the display panel
    auto* panel = gfxDevice->getPanel();
    if (panel)
    {
        panel->setTouch(touch);
        ESP_LOGI(TAG, "Touch panel attached OK");

        // Explicitly initialize touch hardware. device->init() called
        // panel->initTouch() earlier, but touch was null at that point.
        // We must call it now to perform I2C bus init, hardware reset,
        // and touch controller register validation.
        // The FT5x06 may need time after I2C bus init to respond,
        // so retry with delays if the first attempt fails.
        bool touchOk = false;
        for (int attempt = 0; attempt < 5; attempt++)
        {
            if (panel->initTouch())
            {
                ESP_LOGI(TAG, "Touch controller initialized (attempt %d)", attempt + 1);
                touchOk = true;
                break;
            }
            ESP_LOGW(TAG, "initTouch() attempt %d failed, retrying...", attempt + 1);
            lgfx::delay(50);
        }
        if (!touchOk)
        {
            ESP_LOGE(TAG, "Touch controller failed to initialize after all retries");
            // Don't register touch input — polling a non-responsive I2C device
            // causes ~7ms timeout per frame, killing performance
        }
        else
        {
            // Create and register LovyanGFXTouch with input backend
            auto input = std::make_unique<LovyanGFXTouch>();
            input->SetPinInt(pin_int);
            if (input->Initialize())
            {
                DekiInputProvider::SetInput(std::move(input), "LovyanGFXTouch");
                DEKI_LOG_INFO("LGFXTouchPanel: Touch input registered with DekiInputProvider");
            }
            else
            {
                DEKI_LOG_WARNING("LGFXTouchPanel: Failed to initialize LovyanGFXTouch");
            }
        }

        // Mark owner as Persistent so touch persists across prefab changes
        if (GetOwner())
        {
            DekiEngine::GetInstance().GetPrefabSystem().MarkPersistent(GetOwner());
        }

        onComplete(true);
    }
    else
    {
        DEKI_LOG_ERROR("LGFXTouchPanel: No display panel available");
        delete touch;
        onComplete(false);
    }
}

#else // !ESP32 (Editor build)

void LGFXTouchPanel::Setup(SetupCallback onComplete)
{
    // Touch panel is hardware-only; in editor, just report success
    onComplete(true);
}

#endif
