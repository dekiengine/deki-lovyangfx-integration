#include "LGFXDisplayPanel.h"
#include <deki/LogSystem.h>

#if defined(ESP32)
#include <LovyanGFX.hpp>
#include "LovyanGFXDisplay.h"
#include <deki/Engine.h>
#include <deki/SceneSystem.h>
#include "esp_log.h"
#endif

namespace DekiLovyanGfx
{

static lgfx::LGFX_Device* s_LGFXDevice = nullptr;

lgfx::LGFX_Device* LGFXDisplayPanel::GetLGFXDevice()
{
    return s_LGFXDevice;
}

#if defined(ESP32)

static const char* TAG = "LGFXDisplay";

// The package owns the LovyanGFXDisplay; this file-scope pointer keeps it
// alive for the life of the program.
static std::unique_ptr<LovyanGFXDisplay> s_LovyanGFXDisplay;

void LGFXDisplayPanel::Setup(SetupCallback onComplete)
{
    ESP_LOGI(TAG, "Setting up display (panel=%d, bus=%d, %dx%d)", static_cast<int>(panelType),
             static_cast<int>(busType), (int)panelWidth, (int)panelHeight);
    DEKI_LOG_INFO("LGFXDisplayPanel: Setting up display (panel=%d, bus=%d, %dx%d)", static_cast<int>(panelType),
                  static_cast<int>(busType), (int)panelWidth, (int)panelHeight);

    auto* device = new lgfx::LGFX_Device();

    // Log the whole config, to check what the scene file held.
    ESP_LOGI(TAG, "Panel config: invert=%d, rgbOrder=%d, swapBytes=%d", (int)invertColor, (int)rgbOrder,
             (int)swapBytes);
    ESP_LOGI(TAG, "Memory: %dx%d, offset: %d,%d, rotation=%d", (int)memoryWidth, (int)memoryHeight, (int)offsetX,
             (int)offsetY, (int)rotation);
    ESP_LOGI(TAG, "Control pins: CS=%d, RST=%d, BL=%d", (int)csPin, (int)rstPin, (int)blPin);
    if (busType == DisplayBusType::Parallel8bit || busType == DisplayBusType::Parallel16bit)
    {
        ESP_LOGI(TAG, "Parallel pins: RS=%d, WR=%d, RD=%d", (int)rsPin, (int)wrPin, (int)rdPin);
        ESP_LOGI(TAG, "Data pins: D0=%d D1=%d D2=%d D3=%d D4=%d D5=%d D6=%d D7=%d", (int)d0Pin, (int)d1Pin, (int)d2Pin,
                 (int)d3Pin, (int)d4Pin, (int)d5Pin, (int)d6Pin, (int)d7Pin);
    }
    else if (busType == DisplayBusType::SPI)
    {
        ESP_LOGI(TAG, "SPI pins: MOSI=%d, MISO=%d, CLK=%d, DC=%d, host=%d, freq=%d", (int)mosiPin, (int)misoPin,
                 (int)clkPin, (int)dcPin, (int)spiPort, (int)spiWriteHz);
    }
    else if (busType == DisplayBusType::QSPI)
    {
        ESP_LOGI(TAG, "QSPI pins: CLK=%d, IO0=%d, IO1=%d, IO2=%d, IO3=%d, host=%d, freq=%d", (int)clkPin, (int)io0Pin,
                 (int)io1Pin, (int)io2Pin, (int)io3Pin, (int)spiPort, (int)spiWriteHz);
    }

    // The RM67162 speaks only QSPI, and it is the one QSPI panel here.
    if ((panelType == DisplayPanelType::RM67162) != (busType == DisplayBusType::QSPI))
    {
        DEKI_LOG_ERROR("LGFXDisplayPanel: the RM67162 panel and the QSPI bus go together (panel=%d, bus=%d)",
                       static_cast<int>(panelType), static_cast<int>(busType));
        delete device;
        onComplete(false);
        return;
    }

    // --- Bus ---
    if (busType == DisplayBusType::QSPI)
    {
        // LovyanGFX's SPI bus runs in quad mode once all four IO pins are set.
        auto* bus = new lgfx::Bus_SPI();
        auto cfg = bus->config();
        cfg.pin_sclk = clkPin;
        cfg.pin_io0 = io0Pin;
        cfg.pin_io1 = io1Pin;
        cfg.pin_io2 = io2Pin;
        cfg.pin_io3 = io3Pin;
        cfg.spi_host = static_cast<spi_host_device_t>(spiPort);
        cfg.spi_mode = 0;
        cfg.freq_write = spiWriteHz;
        bus->config(cfg);
        DEKI_LOG_INFO("LGFXDisplayPanel: QSPI bus configured (CLK=%d, IO0..3=%d,%d,%d,%d)", (int)clkPin, (int)io0Pin,
                      (int)io1Pin, (int)io2Pin, (int)io3Pin);

        auto* panel = new lgfx::Panel_RM67162();
        auto panelCfg = panel->config();
        panelCfg.pin_cs = csPin;
        panelCfg.pin_rst = rstPin;
        panelCfg.pin_busy = -1;
        panelCfg.panel_width = panelWidth;
        panelCfg.panel_height = panelHeight;
        panelCfg.memory_width = memoryWidth;
        panelCfg.memory_height = memoryHeight;
        panelCfg.offset_x = offsetX;
        panelCfg.offset_y = offsetY;
        panelCfg.offset_rotation = 0;
        panelCfg.readable = true;
        panel->config(panelCfg);
        panel->setBus(bus);

        // No backlight: an AMOLED's brightness is a panel command.
        device->setPanel(panel);
    }
    else if (busType == DisplayBusType::SPI)
    {
        auto* bus = new lgfx::Bus_SPI();
        auto cfg = bus->config();
        cfg.pin_mosi = mosiPin;
        cfg.pin_miso = misoPin;
        cfg.pin_sclk = clkPin;
        cfg.pin_dc = dcPin;
        cfg.spi_host = static_cast<spi_host_device_t>(spiPort);
        cfg.freq_write = spiWriteHz;
        bus->config(cfg);
        device->setPanel(nullptr);  // clear before setting the bus
        // The bus is set on the panel below.
        DEKI_LOG_INFO("LGFXDisplayPanel: SPI bus configured (MOSI=%d, CLK=%d, DC=%d)", (int)mosiPin, (int)clkPin,
                      (int)dcPin);

        // Create panel and set bus
        lgfx::Panel_Device* panel = nullptr;
        switch (panelType)
        {
            case DisplayPanelType::ILI9341: panel = new lgfx::Panel_ILI9341(); break;
            case DisplayPanelType::ST7789: panel = new lgfx::Panel_ST7789(); break;
            case DisplayPanelType::ST7735: panel = new lgfx::Panel_ST7735(); break;
            case DisplayPanelType::GC9A01: panel = new lgfx::Panel_GC9A01(); break;
            case DisplayPanelType::SSD1351: panel = new lgfx::Panel_SSD1351(); break;
            case DisplayPanelType::ST7789P3: panel = new lgfx::Panel_ST7789P3(); break;
            default:
                DEKI_LOG_ERROR("LGFXDisplayPanel: Unknown panel type %d", static_cast<int>(panelType));
                delete bus;
                delete device;
                onComplete(false);
                return;
        }

        auto panelCfg = panel->config();
        panelCfg.pin_cs = csPin;
        panelCfg.pin_rst = rstPin;
        panelCfg.pin_busy = -1;
        panelCfg.panel_width = panelWidth;
        panelCfg.panel_height = panelHeight;
        panelCfg.memory_width = memoryWidth;
        panelCfg.memory_height = memoryHeight;
        panelCfg.offset_x = offsetX;
        panelCfg.offset_y = offsetY;
        panelCfg.offset_rotation = 0;
        panelCfg.readable = true;
        panelCfg.invert = invertColor;
        panelCfg.rgb_order = rgbOrder;
        panel->config(panelCfg);
        panel->setBus(bus);

        // Backlight
        if (blPin >= 0)
        {
            auto* light = new lgfx::Light_PWM();
            auto lightCfg = light->config();
            lightCfg.pin_bl = blPin;
            lightCfg.pwm_channel = blPwmChannel;
            lightCfg.invert = blInvert;
            light->config(lightCfg);
            panel->setLight(light);
        }

        device->setPanel(panel);
    }
    else if (busType == DisplayBusType::Parallel8bit)
    {
        auto* bus = new lgfx::Bus_Parallel8();
        auto cfg = bus->config();
        cfg.freq_write = parWriteHz;
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
        DEKI_LOG_INFO("LGFXDisplayPanel: Parallel8 bus configured (RS=%d, WR=%d, RD=%d, D0=%d..D7=%d)", (int)rsPin,
                      (int)wrPin, (int)rdPin, (int)d0Pin, (int)d7Pin);

        // Create panel and set bus
        lgfx::Panel_Device* panel = nullptr;
        switch (panelType)
        {
            case DisplayPanelType::ILI9341: panel = new lgfx::Panel_ILI9341(); break;
            case DisplayPanelType::ST7789: panel = new lgfx::Panel_ST7789(); break;
            case DisplayPanelType::ST7735: panel = new lgfx::Panel_ST7735(); break;
            case DisplayPanelType::GC9A01: panel = new lgfx::Panel_GC9A01(); break;
            case DisplayPanelType::SSD1351: panel = new lgfx::Panel_SSD1351(); break;
            case DisplayPanelType::ST7789P3: panel = new lgfx::Panel_ST7789P3(); break;
            default:
                DEKI_LOG_ERROR("LGFXDisplayPanel: Unknown panel type %d", static_cast<int>(panelType));
                delete bus;
                delete device;
                onComplete(false);
                return;
        }
        auto panelCfg = panel->config();
        panelCfg.pin_cs = csPin;
        panelCfg.pin_rst = rstPin;
        panelCfg.pin_busy = -1;
        panelCfg.panel_width = panelWidth;
        panelCfg.panel_height = panelHeight;
        panelCfg.memory_width = memoryWidth;
        panelCfg.memory_height = memoryHeight;
        panelCfg.offset_x = offsetX;
        panelCfg.offset_y = offsetY;
        panelCfg.offset_rotation = 0;
        panelCfg.readable = true;
        panelCfg.invert = invertColor;
        panelCfg.rgb_order = rgbOrder;
        panel->config(panelCfg);
        panel->setBus(bus);

        // Backlight
        if (blPin >= 0)
        {
            auto* light = new lgfx::Light_PWM();
            auto lightCfg = light->config();
            lightCfg.pin_bl = blPin;
            lightCfg.pwm_channel = blPwmChannel;
            lightCfg.invert = blInvert;
            light->config(lightCfg);
            panel->setLight(light);
        }

        device->setPanel(panel);
    }
    else if (busType == DisplayBusType::Parallel16bit)
    {
        auto* bus = new lgfx::Bus_Parallel16();
        auto cfg = bus->config();
        cfg.freq_write = parWriteHz;
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
        cfg.pin_d8 = d8Pin;
        cfg.pin_d9 = d9Pin;
        cfg.pin_d10 = d10Pin;
        cfg.pin_d11 = d11Pin;
        cfg.pin_d12 = d12Pin;
        cfg.pin_d13 = d13Pin;
        cfg.pin_d14 = d14Pin;
        cfg.pin_d15 = d15Pin;
        bus->config(cfg);
        DEKI_LOG_INFO("LGFXDisplayPanel: Parallel16 bus configured (RS=%d, WR=%d, D0=%d..D15=%d)", (int)rsPin,
                      (int)wrPin, (int)d0Pin, (int)d15Pin);

        // Create panel and set bus
        lgfx::Panel_Device* panel = nullptr;
        switch (panelType)
        {
            case DisplayPanelType::ILI9341: panel = new lgfx::Panel_ILI9341(); break;
            case DisplayPanelType::ST7789: panel = new lgfx::Panel_ST7789(); break;
            case DisplayPanelType::ST7735: panel = new lgfx::Panel_ST7735(); break;
            case DisplayPanelType::GC9A01: panel = new lgfx::Panel_GC9A01(); break;
            case DisplayPanelType::SSD1351: panel = new lgfx::Panel_SSD1351(); break;
            case DisplayPanelType::ST7789P3: panel = new lgfx::Panel_ST7789P3(); break;
            default:
                DEKI_LOG_ERROR("LGFXDisplayPanel: Unknown panel type %d", static_cast<int>(panelType));
                delete bus;
                delete device;
                onComplete(false);
                return;
        }

        auto panelCfg = panel->config();
        panelCfg.pin_cs = csPin;
        panelCfg.pin_rst = rstPin;
        panelCfg.pin_busy = -1;
        panelCfg.panel_width = panelWidth;
        panelCfg.panel_height = panelHeight;
        panelCfg.memory_width = memoryWidth;
        panelCfg.memory_height = memoryHeight;
        panelCfg.offset_x = offsetX;
        panelCfg.offset_y = offsetY;
        panelCfg.offset_rotation = 0;
        panelCfg.readable = true;
        panelCfg.invert = invertColor;
        panelCfg.rgb_order = rgbOrder;
        panelCfg.dlen_16bit = true;
        panel->config(panelCfg);
        panel->setBus(bus);

        // Backlight
        if (blPin >= 0)
        {
            auto* light = new lgfx::Light_PWM();
            auto lightCfg = light->config();
            lightCfg.pin_bl = blPin;
            lightCfg.pwm_channel = blPwmChannel;
            lightCfg.invert = blInvert;
            light->config(lightCfg);
            panel->setLight(light);
        }

        device->setPanel(panel);
    }

    if (!device->init())
    {
        ESP_LOGE(TAG, "device->init() failed");
        delete device;
        onComplete(false);
        return;
    }

    device->setRotation(static_cast<uint8_t>(rotation));
    ESP_LOGI(TAG, "Display initialized (%dx%d, rotation=%d)", (int)panelWidth, (int)panelHeight, (int)rotation);

    s_LGFXDevice = device;

    // Wrap the device in a LovyanGFXDisplay and give it to the engine.
    s_LovyanGFXDisplay = std::make_unique<LovyanGFXDisplay>();
    if (!s_LovyanGFXDisplay->InitializeWithDevice(device, device->width(), device->height(), swapBytes, usePsram,
                                                  doubleBuffer))
    {
        DEKI_LOG_ERROR("LGFXDisplayPanel: Failed to initialize display wrapper");
        s_LovyanGFXDisplay.reset();
        onComplete(false);
        return;
    }

    // The RM67162 ignores a write whose start or size is odd along its short
    // axis. Full-width rows are always even across; this keeps them even down.
    if (panelType == DisplayPanelType::RM67162)
    {
        s_LovyanGFXDisplay->SetRowAlignment(2);
    }

    Deki::Engine::GetInstance().SetDisplay(s_LovyanGFXDisplay.get(), "LovyanGFX");

    // Make the owner persistent, so the display survives scene changes.
    if (GetOwner())
    {
        Deki::Engine::GetInstance().GetSceneSystem().MarkPersistent(GetOwner());
    }

    onComplete(true);
}

#else  // !ESP32 (editor build)

void LGFXDisplayPanel::Setup(SetupCallback onComplete)
{
    // There is no display hardware in the editor; report success.
    onComplete(true);
}

#endif

}  // namespace DekiLovyanGfx
