#include "LGFXDisplayPanel.h"
#include "DekiLogSystem.h"

// Static LGFX device instance
static lgfx::LGFX_Device* s_LGFXDevice = nullptr;

lgfx::LGFX_Device* LGFXDisplayPanel::GetLGFXDevice()
{
    return s_LGFXDevice;
}

#if defined(ESP32)

#include <LovyanGFX.hpp>
#include "LovyanGFXDisplay.h"
#include "DekiEngine.h"
#include "SceneSystem.h"
#include "esp_log.h"
static const char* TAG = "LGFXDisplay";

// Package owns the LovyanGFXDisplay lifetime now. File-scope unique_ptr keeps
// it alive for the program's lifetime.
static std::unique_ptr<LovyanGFXDisplay> s_LovyanGFXDisplay;

void LGFXDisplayPanel::Setup(SetupCallback onComplete)
{
    ESP_LOGI(TAG, "Setting up display (panel=%d, bus=%d, %dx%d)",
             static_cast<int>(panelType), static_cast<int>(busType),
             (int)panelWidth, (int)panelHeight);
    DEKI_LOG_INFO("LGFXDisplayPanel: Setting up display (panel=%d, bus=%d, %dx%d)",
                  static_cast<int>(panelType), static_cast<int>(busType),
                  (int)panelWidth, (int)panelHeight);

    auto* device = new lgfx::LGFX_Device();

    // Dump all config so we can verify the msgpack scene contents
    ESP_LOGI(TAG, "Panel config: invert=%d, rgbOrder=%d, swapBytes=%d",
             (int)invertColor, (int)rgbOrder, (int)swapBytes);
    ESP_LOGI(TAG, "Memory: %dx%d, offset: %d,%d, rotation=%d",
             (int)memoryWidth, (int)memoryHeight, (int)offsetX, (int)offsetY, (int)rotation);
    ESP_LOGI(TAG, "Control pins: CS=%d, RST=%d, BL=%d",
             (int)pinCs, (int)pinRst, (int)blPin);
    if (busType == DisplayBusType::Parallel8bit || busType == DisplayBusType::Parallel16bit)
    {
        ESP_LOGI(TAG, "Parallel pins: RS=%d, WR=%d, RD=%d",
                 (int)rsPin, (int)wrPin, (int)rdPin);
        ESP_LOGI(TAG, "Data pins: D0=%d D1=%d D2=%d D3=%d D4=%d D5=%d D6=%d D7=%d",
                 (int)d0Pin, (int)d1Pin, (int)d2Pin, (int)d3Pin,
                 (int)d4Pin, (int)d5Pin, (int)d6Pin, (int)d7Pin);
    }
    else if (busType == DisplayBusType::SPI)
    {
        ESP_LOGI(TAG, "SPI pins: MOSI=%d, MISO=%d, CLK=%d, DC=%d, host=%d, freq=%d",
                 (int)spiMosi, (int)spiMiso, (int)spiClk, (int)spiDc,
                 (int)spiHost, (int)spiFreqWrite);
    }

    // --- Configure Bus ---
    if (busType == DisplayBusType::SPI)
    {
        auto* bus = new lgfx::Bus_SPI();
        auto cfg = bus->config();
        cfg.pin_mosi = spiMosi;
        cfg.pin_miso = spiMiso;
        cfg.pin_sclk = spiClk;
        cfg.pin_dc = spiDc;
        cfg.spiHost = static_cast<spi_host_device_t>(spiHost);
        cfg.freq_write = spiFreqWrite;
        bus->config(cfg);
        device->setPanel(nullptr); // Clear before setting bus
        // Bus gets set on the panel below
        DEKI_LOG_INFO("LGFXDisplayPanel: SPI bus configured (MOSI=%d, CLK=%d, DC=%d)",
                      (int)spiMosi, (int)spiClk, (int)spiDc);

        // Create panel and set bus
        lgfx::Panel_Device* panel = nullptr;
        switch (panelType)
        {
            case DisplayPanelType::ILI9341:  panel = new lgfx::Panel_ILI9341();  break;
            case DisplayPanelType::ST7789:   panel = new lgfx::Panel_ST7789();   break;
            case DisplayPanelType::ST7735:   panel = new lgfx::Panel_ST7735();   break;
            case DisplayPanelType::GC9A01:   panel = new lgfx::Panel_GC9A01();   break;
            case DisplayPanelType::SSD1351:  panel = new lgfx::Panel_SSD1351();  break;
            case DisplayPanelType::ST7789P3: panel = new lgfx::Panel_ST7789P3(); break;
            default:
                DEKI_LOG_ERROR("LGFXDisplayPanel: Unknown panel type %d", static_cast<int>(panelType));
                delete bus;
                delete device;
                onComplete(false);
                return;
        }

        auto panel_cfg = panel->config();
        panel_cfg.pinCs = pinCs;
        panel_cfg.pinRst = pinRst;
        panel_cfg.pin_busy = -1;
        panel_cfg.panelWidth = panelWidth;
        panel_cfg.panelHeight = panelHeight;
        panel_cfg.memoryWidth = memoryWidth;
        panel_cfg.memoryHeight = memoryHeight;
        panel_cfg.offsetX = offsetX;
        panel_cfg.offsetY = offsetY;
        panel_cfg.offsetRotation = 0;
        panel_cfg.readable = true;
        panel_cfg.invert = invertColor;
        panel_cfg.rgbOrder = rgbOrder;
        panel->config(panel_cfg);
        panel->setBus(bus);

        // Backlight
        if (blPin >= 0)
        {
            auto* light = new lgfx::Light_PWM();
            auto light_cfg = light->config();
            light_cfg.pin_bl = blPin;
            light_cfg.pwm_channel = blPwmChannel;
            light_cfg.invert = blInvert;
            light->config(light_cfg);
            panel->setLight(light);
        }

        device->setPanel(panel);
    }
    else if (busType == DisplayBusType::Parallel8bit)
    {
        auto* bus = new lgfx::Bus_Parallel8();
        auto cfg = bus->config();
        cfg.freq_write = parFreqWrite;
        cfg.pin_rs = rsPin;
        cfg.pin_wr = wrPin;
        cfg.pin_rd = rdPin;
        cfg.pin_d0 = d0Pin;
        cfg.pin_d1 = d1Pin;
        cfg.pin_d2 = d2Pin;
        cfg.pin_d3 = d3Pin;
        cfg.pin_d4 = d4Pin;
        cfg.pin_d5 = d5Pin;
        cfg.pin_d6 = d6Pin;
        cfg.pin_d7 = d7Pin;
        bus->config(cfg);
        DEKI_LOG_INFO("LGFXDisplayPanel: Parallel8 bus configured (RS=%d, WR=%d, RD=%d, D0=%d..D7=%d)",
                      (int)rsPin, (int)wrPin, (int)rdPin, (int)d0Pin, (int)d7Pin);

        // Create panel and set bus
        lgfx::Panel_Device* panel = nullptr;
        switch (panelType)
        {
            case DisplayPanelType::ILI9341:  panel = new lgfx::Panel_ILI9341();  break;
            case DisplayPanelType::ST7789:   panel = new lgfx::Panel_ST7789();   break;
            case DisplayPanelType::ST7735:   panel = new lgfx::Panel_ST7735();   break;
            case DisplayPanelType::GC9A01:   panel = new lgfx::Panel_GC9A01();   break;
            case DisplayPanelType::SSD1351:  panel = new lgfx::Panel_SSD1351();  break;
            case DisplayPanelType::ST7789P3: panel = new lgfx::Panel_ST7789P3(); break;
            default:
                DEKI_LOG_ERROR("LGFXDisplayPanel: Unknown panel type %d", static_cast<int>(panelType));
                delete bus;
                delete device;
                onComplete(false);
                return;
        }
        auto panel_cfg = panel->config();
        panel_cfg.pinCs = pinCs;
        panel_cfg.pinRst = pinRst;
        panel_cfg.pin_busy = -1;
        panel_cfg.panelWidth = panelWidth;
        panel_cfg.panelHeight = panelHeight;
        panel_cfg.memoryWidth = memoryWidth;
        panel_cfg.memoryHeight = memoryHeight;
        panel_cfg.offsetX = offsetX;
        panel_cfg.offsetY = offsetY;
        panel_cfg.offsetRotation = 0;
        panel_cfg.readable = true;
        panel_cfg.invert = invertColor;
        panel_cfg.rgbOrder = rgbOrder;
        panel->config(panel_cfg);
        panel->setBus(bus);

        // Backlight
        if (blPin >= 0)
        {
            auto* light = new lgfx::Light_PWM();
            auto light_cfg = light->config();
            light_cfg.pin_bl = blPin;
            light_cfg.pwm_channel = blPwmChannel;
            light_cfg.invert = blInvert;
            light->config(light_cfg);
            panel->setLight(light);
        }

        device->setPanel(panel);
    }
    else if (busType == DisplayBusType::Parallel16bit)
    {
        auto* bus = new lgfx::Bus_Parallel16();
        auto cfg = bus->config();
        cfg.freq_write = parFreqWrite;
        cfg.pin_rs = rsPin;
        cfg.pin_wr = wrPin;
        cfg.pin_rd = rdPin;
        cfg.pin_d0  = d0Pin;
        cfg.pin_d1  = d1Pin;
        cfg.pin_d2  = d2Pin;
        cfg.pin_d3  = d3Pin;
        cfg.pin_d4  = d4Pin;
        cfg.pin_d5  = d5Pin;
        cfg.pin_d6  = d6Pin;
        cfg.pin_d7  = d7Pin;
        cfg.pin_d8  = d8Pin;
        cfg.pin_d9  = d9Pin;
        cfg.pin_d10 = d10Pin;
        cfg.pin_d11 = d11Pin;
        cfg.pin_d12 = d12Pin;
        cfg.pin_d13 = d13Pin;
        cfg.pin_d14 = d14Pin;
        cfg.pin_d15 = d15Pin;
        bus->config(cfg);
        DEKI_LOG_INFO("LGFXDisplayPanel: Parallel16 bus configured (RS=%d, WR=%d, D0=%d..D15=%d)",
                      (int)rsPin, (int)wrPin, (int)d0Pin, (int)d15Pin);

        // Create panel and set bus
        lgfx::Panel_Device* panel = nullptr;
        switch (panelType)
        {
            case DisplayPanelType::ILI9341:  panel = new lgfx::Panel_ILI9341();  break;
            case DisplayPanelType::ST7789:   panel = new lgfx::Panel_ST7789();   break;
            case DisplayPanelType::ST7735:   panel = new lgfx::Panel_ST7735();   break;
            case DisplayPanelType::GC9A01:   panel = new lgfx::Panel_GC9A01();   break;
            case DisplayPanelType::SSD1351:  panel = new lgfx::Panel_SSD1351();  break;
            case DisplayPanelType::ST7789P3: panel = new lgfx::Panel_ST7789P3(); break;
            default:
                DEKI_LOG_ERROR("LGFXDisplayPanel: Unknown panel type %d", static_cast<int>(panelType));
                delete bus;
                delete device;
                onComplete(false);
                return;
        }

        auto panel_cfg = panel->config();
        panel_cfg.pinCs = pinCs;
        panel_cfg.pinRst = pinRst;
        panel_cfg.pin_busy = -1;
        panel_cfg.panelWidth = panelWidth;
        panel_cfg.panelHeight = panelHeight;
        panel_cfg.memoryWidth = memoryWidth;
        panel_cfg.memoryHeight = memoryHeight;
        panel_cfg.offsetX = offsetX;
        panel_cfg.offsetY = offsetY;
        panel_cfg.offsetRotation = 0;
        panel_cfg.readable = true;
        panel_cfg.invert = invertColor;
        panel_cfg.rgbOrder = rgbOrder;
        panel_cfg.dlen_16bit = true;
        panel->config(panel_cfg);
        panel->setBus(bus);

        // Backlight
        if (blPin >= 0)
        {
            auto* light = new lgfx::Light_PWM();
            auto light_cfg = light->config();
            light_cfg.pin_bl = blPin;
            light_cfg.pwm_channel = blPwmChannel;
            light_cfg.invert = blInvert;
            light->config(light_cfg);
            panel->setLight(light);
        }

        device->setPanel(panel);
    }

    // Initialize the display hardware
    if (!device->init())
    {
        ESP_LOGE(TAG, "device->init() failed");
        delete device;
        onComplete(false);
        return;
    }

    device->setRotation(static_cast<uint8_t>(rotation));
    ESP_LOGI(TAG, "Display initialized (%dx%d, rotation=%d)", (int)panelWidth, (int)panelHeight, (int)rotation);

    // Store for static accessor
    s_LGFXDevice = device;

    // Create LovyanGFXDisplay wrapper and register with engine
    s_LovyanGFXDisplay = std::make_unique<LovyanGFXDisplay>();
    if (!s_LovyanGFXDisplay->InitializeWithDevice(device, device->width(), device->height(), swapBytes, usePsram, doubleBuffer))
    {
        DEKI_LOG_ERROR("LGFXDisplayPanel: Failed to initialize display wrapper");
        s_LovyanGFXDisplay.reset();
        onComplete(false);
        return;
    }

    DekiEngine::GetInstance().SetDisplay(s_LovyanGFXDisplay.get(), "LovyanGFX");

    // Mark owner as Persistent so display persists across scene changes
    if (GetOwner())
    {
        DekiEngine::GetInstance().GetSceneSystem().MarkPersistent(GetOwner());
    }

    onComplete(true);
}

#else // !ESP32 (Editor build)

void LGFXDisplayPanel::Setup(SetupCallback onComplete)
{
    // Display setup is hardware-only; in editor, just report success
    onComplete(true);
}

#endif
