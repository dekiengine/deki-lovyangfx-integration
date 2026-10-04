#include "LGFXTouchPanel.h"
#include <deki/LogSystem.h>
#include <deki/Engine.h>
#include <deki/SceneSystem.h>
#include "DekiInput.h"  // from deki-input
#include "DekiI2C.h"    // from deki-i2c
#include "IDekiI2C.h"   // from deki-i2c

#if defined(ESP32)
#include <LovyanGFX.hpp>
#include "LGFXDisplayPanel.h"
#include "LovyanGFXTouch.h"
#include "esp_log.h"
#endif

namespace DekiLovyanGfx
{

#if defined(ESP32)

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

    // Find the shared I2C bus (capacitive drivers only; XPT2046 is SPI).
    int busSda = -1;
    int busScl = -1;
    int busFreq = 0;
    const bool isI2cDriver = driverType == TouchDriverType::FT5x06 || driverType == TouchDriverType::GT911 ||
                             driverType == TouchDriverType::CST816S;

    if (isI2cDriver)
    {
        DekiI2c::IDekiI2C* bus = DekiI2c::DekiI2C::GetBus(i2cPort);
        if (!bus)
        {
            DEKI_LOG_ERROR("LGFXTouchPanel: no I2C bus on port %d — add DekiI2c::I2CBusComponent before LGFXTouchPanel "
                           "in boot scene",
                           (int)i2cPort);
            onComplete(false);
            return;
        }
        busSda = bus->GetSdaPin();
        busScl = bus->GetSclPin();
        busFreq = bus->GetFrequencyHz();
    }

    lgfx::ITouch* touch = nullptr;

    // Create the touch driver.
    switch (driverType)
    {
        case TouchDriverType::FT5x06:
        {
            auto* t = new lgfx::Touch_FT5x06();
            auto cfg = t->config();
            cfg.x_min = xMin;
            cfg.x_max = xMax;
            cfg.y_min = yMin;
            cfg.y_max = yMax;
            cfg.pin_int = intPin;
            cfg.pin_rst = rstPin;
            cfg.pin_sda = busSda;
            cfg.pin_scl = busScl;
            cfg.i2c_addr = 0x38;
            cfg.i2c_port = i2cPort;
            cfg.freq = busFreq;
            cfg.bus_shared = true;
            cfg.offset_rotation = static_cast<uint8_t>(offsetRotation);
            t->config(cfg);
            touch = t;
            DEKI_LOG_INFO("LGFXTouchPanel: FT5x06 on I2C port %d (SDA=%d SCL=%d freq=%d)", (int)i2cPort, busSda, busScl,
                          busFreq);
            break;
        }
        case TouchDriverType::GT911:
        {
            auto* t = new lgfx::Touch_GT911();
            auto cfg = t->config();
            cfg.x_min = xMin;
            cfg.x_max = xMax;
            cfg.y_min = yMin;
            cfg.y_max = yMax;
            cfg.pin_int = intPin;
            cfg.pin_rst = rstPin;
            cfg.pin_sda = busSda;
            cfg.pin_scl = busScl;
            cfg.i2c_addr = 0x5D;
            cfg.i2c_port = i2cPort;
            cfg.freq = busFreq;
            cfg.bus_shared = true;
            cfg.offset_rotation = static_cast<uint8_t>(offsetRotation);
            t->config(cfg);
            touch = t;
            DEKI_LOG_INFO("LGFXTouchPanel: GT911 on I2C port %d (SDA=%d SCL=%d freq=%d)", (int)i2cPort, busSda, busScl,
                          busFreq);
            break;
        }
        case TouchDriverType::CST816S:
        {
            auto* t = new lgfx::Touch_CST816S();
            auto cfg = t->config();
            cfg.x_min = xMin;
            cfg.x_max = xMax;
            cfg.y_min = yMin;
            cfg.y_max = yMax;
            cfg.pin_int = intPin;
            cfg.pin_rst = rstPin;
            cfg.pin_sda = busSda;
            cfg.pin_scl = busScl;
            cfg.i2c_addr = 0x15;
            cfg.i2c_port = i2cPort;
            cfg.freq = busFreq;
            cfg.bus_shared = true;
            cfg.offset_rotation = static_cast<uint8_t>(offsetRotation);
            t->config(cfg);
            touch = t;
            DEKI_LOG_INFO("LGFXTouchPanel: CST816S on I2C port %d (SDA=%d SCL=%d freq=%d)", (int)i2cPort, busSda,
                          busScl, busFreq);
            break;
        }
        case TouchDriverType::XPT2046:
        {
            auto* t = new lgfx::Touch_XPT2046();
            auto cfg = t->config();
            cfg.x_min = xMin;
            cfg.x_max = xMax;
            cfg.y_min = yMin;
            cfg.y_max = yMax;
            cfg.pin_int = intPin;
            cfg.pin_rst = rstPin;
            cfg.pin_cs = csPin;
            cfg.pin_mosi = mosiPin;
            cfg.pin_miso = misoPin;
            cfg.pin_sclk = clkPin;
            cfg.bus_shared = busShared;
            cfg.offset_rotation = static_cast<uint8_t>(offsetRotation);
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

    // Attach the touch controller to the display panel.
    auto* panel = gfxDevice->getPanel();
    if (panel)
    {
        panel->setTouch(touch);
        ESP_LOGI(TAG, "Touch panel attached OK");

        // Start the touch hardware now. device->init() already called
        // panel->initTouch(), but touch was null then, so this call does the
        // I2C bus init, the hardware reset and the controller register check.
        // The FT5x06 may need time after I2C bus init to respond, so retry
        // with delays if the first attempt fails.
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
            // Do not register touch input: polling an I2C device that does not
            // answer costs a ~7 ms timeout every frame.
        }
        else
        {
            // Create LovyanGFXTouch and register it as the input backend.
            auto input = std::make_unique<LovyanGFXTouch>();
            input->SetPinInt(intPin);
            if (input->Initialize())
            {
                using DekiInputApi = DekiInput::DekiInput;
                DekiInputApi::SetInput(std::move(input), "LovyanGFXTouch");
                DEKI_LOG_INFO("LGFXTouchPanel: Touch input registered with DekiInput");
            }
            else
            {
                DEKI_LOG_WARNING("LGFXTouchPanel: Failed to initialize LovyanGFXTouch");
            }
        }

        // Make the owner persistent, so touch survives scene changes.
        if (GetOwner())
        {
            Deki::Engine::GetInstance().GetSceneSystem().MarkPersistent(GetOwner());
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

#else  // !ESP32 (editor build)

void LGFXTouchPanel::Setup(SetupCallback onComplete)
{
    // There is no touch hardware in the editor; report success.
    onComplete(true);
}

#endif

}  // namespace DekiLovyanGfx
