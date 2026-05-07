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
#include "providers/DekiDisplay.h"
#include "DekiEngine.h"
#include "PrefabSystem.h"
#include "esp_log.h"
static const char* TAG = "LGFXDisplay";

void LGFXDisplayPanel::Setup(SetupCallback onComplete)
{
    ESP_LOGI(TAG, "Setting up display (panel=%d, bus=%d, %dx%d)",
             static_cast<int>(panelType), static_cast<int>(busType),
             (int)panel_width, (int)panel_height);
    DEKI_LOG_INFO("LGFXDisplayPanel: Setting up display (panel=%d, bus=%d, %dx%d)",
                  static_cast<int>(panelType), static_cast<int>(busType),
                  (int)panel_width, (int)panel_height);

    auto* device = new lgfx::LGFX_Device();

    // Dump all config so we can verify the msgpack prefab contents
    ESP_LOGI(TAG, "Panel config: invert=%d, rgb_order=%d, swap_bytes=%d",
             (int)invert_color, (int)rgb_order, (int)swap_bytes);
    ESP_LOGI(TAG, "Memory: %dx%d, offset: %d,%d, rotation=%d",
             (int)memory_width, (int)memory_height, (int)offset_x, (int)offset_y, (int)rotation);
    ESP_LOGI(TAG, "Control pins: CS=%d, RST=%d, BL=%d",
             (int)pin_cs, (int)pin_rst, (int)bl_pin);
    if (busType == DisplayBusType::Parallel8bit || busType == DisplayBusType::Parallel16bit)
    {
        ESP_LOGI(TAG, "Parallel pins: RS=%d, WR=%d, RD=%d",
                 (int)rs_pin, (int)wr_pin, (int)rd_pin);
        ESP_LOGI(TAG, "Data pins: D0=%d D1=%d D2=%d D3=%d D4=%d D5=%d D6=%d D7=%d",
                 (int)d0_pin, (int)d1_pin, (int)d2_pin, (int)d3_pin,
                 (int)d4_pin, (int)d5_pin, (int)d6_pin, (int)d7_pin);
    }
    else if (busType == DisplayBusType::SPI)
    {
        ESP_LOGI(TAG, "SPI pins: MOSI=%d, MISO=%d, CLK=%d, DC=%d, host=%d, freq=%d",
                 (int)spi_mosi, (int)spi_miso, (int)spi_clk, (int)spi_dc,
                 (int)spi_host, (int)spi_freq_write);
    }

    // --- Configure Bus ---
    if (busType == DisplayBusType::SPI)
    {
        auto* bus = new lgfx::Bus_SPI();
        auto cfg = bus->config();
        cfg.pin_mosi = spi_mosi;
        cfg.pin_miso = spi_miso;
        cfg.pin_sclk = spi_clk;
        cfg.pin_dc = spi_dc;
        cfg.spi_host = static_cast<spi_host_device_t>(spi_host);
        cfg.freq_write = spi_freq_write;
        bus->config(cfg);
        device->setPanel(nullptr); // Clear before setting bus
        // Bus gets set on the panel below
        DEKI_LOG_INFO("LGFXDisplayPanel: SPI bus configured (MOSI=%d, CLK=%d, DC=%d)",
                      (int)spi_mosi, (int)spi_clk, (int)spi_dc);

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
        panel_cfg.pin_cs = pin_cs;
        panel_cfg.pin_rst = pin_rst;
        panel_cfg.pin_busy = -1;
        panel_cfg.panel_width = panel_width;
        panel_cfg.panel_height = panel_height;
        panel_cfg.memory_width = memory_width;
        panel_cfg.memory_height = memory_height;
        panel_cfg.offset_x = offset_x;
        panel_cfg.offset_y = offset_y;
        panel_cfg.offset_rotation = 0;
        panel_cfg.readable = true;
        panel_cfg.invert = invert_color;
        panel_cfg.rgb_order = rgb_order;
        panel->config(panel_cfg);
        panel->setBus(bus);

        // Backlight
        if (bl_pin >= 0)
        {
            auto* light = new lgfx::Light_PWM();
            auto light_cfg = light->config();
            light_cfg.pin_bl = bl_pin;
            light_cfg.pwm_channel = bl_pwm_channel;
            light_cfg.invert = bl_invert;
            light->config(light_cfg);
            panel->setLight(light);
        }

        device->setPanel(panel);
    }
    else if (busType == DisplayBusType::Parallel8bit)
    {
        auto* bus = new lgfx::Bus_Parallel8();
        auto cfg = bus->config();
        cfg.freq_write = par_freq_write;
        cfg.pin_rs = rs_pin;
        cfg.pin_wr = wr_pin;
        cfg.pin_rd = rd_pin;
        cfg.pin_d0 = d0_pin;
        cfg.pin_d1 = d1_pin;
        cfg.pin_d2 = d2_pin;
        cfg.pin_d3 = d3_pin;
        cfg.pin_d4 = d4_pin;
        cfg.pin_d5 = d5_pin;
        cfg.pin_d6 = d6_pin;
        cfg.pin_d7 = d7_pin;
        bus->config(cfg);
        DEKI_LOG_INFO("LGFXDisplayPanel: Parallel8 bus configured (RS=%d, WR=%d, RD=%d, D0=%d..D7=%d)",
                      (int)rs_pin, (int)wr_pin, (int)rd_pin, (int)d0_pin, (int)d7_pin);

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
        panel_cfg.pin_cs = pin_cs;
        panel_cfg.pin_rst = pin_rst;
        panel_cfg.pin_busy = -1;
        panel_cfg.panel_width = panel_width;
        panel_cfg.panel_height = panel_height;
        panel_cfg.memory_width = memory_width;
        panel_cfg.memory_height = memory_height;
        panel_cfg.offset_x = offset_x;
        panel_cfg.offset_y = offset_y;
        panel_cfg.offset_rotation = 0;
        panel_cfg.readable = true;
        panel_cfg.invert = invert_color;
        panel_cfg.rgb_order = rgb_order;
        panel->config(panel_cfg);
        panel->setBus(bus);

        // Backlight
        if (bl_pin >= 0)
        {
            auto* light = new lgfx::Light_PWM();
            auto light_cfg = light->config();
            light_cfg.pin_bl = bl_pin;
            light_cfg.pwm_channel = bl_pwm_channel;
            light_cfg.invert = bl_invert;
            light->config(light_cfg);
            panel->setLight(light);
        }

        device->setPanel(panel);
    }
    else if (busType == DisplayBusType::Parallel16bit)
    {
        auto* bus = new lgfx::Bus_Parallel16();
        auto cfg = bus->config();
        cfg.freq_write = par_freq_write;
        cfg.pin_rs = rs_pin;
        cfg.pin_wr = wr_pin;
        cfg.pin_rd = rd_pin;
        cfg.pin_d0  = d0_pin;
        cfg.pin_d1  = d1_pin;
        cfg.pin_d2  = d2_pin;
        cfg.pin_d3  = d3_pin;
        cfg.pin_d4  = d4_pin;
        cfg.pin_d5  = d5_pin;
        cfg.pin_d6  = d6_pin;
        cfg.pin_d7  = d7_pin;
        cfg.pin_d8  = d8_pin;
        cfg.pin_d9  = d9_pin;
        cfg.pin_d10 = d10_pin;
        cfg.pin_d11 = d11_pin;
        cfg.pin_d12 = d12_pin;
        cfg.pin_d13 = d13_pin;
        cfg.pin_d14 = d14_pin;
        cfg.pin_d15 = d15_pin;
        bus->config(cfg);
        DEKI_LOG_INFO("LGFXDisplayPanel: Parallel16 bus configured (RS=%d, WR=%d, D0=%d..D15=%d)",
                      (int)rs_pin, (int)wr_pin, (int)d0_pin, (int)d15_pin);

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
        panel_cfg.pin_cs = pin_cs;
        panel_cfg.pin_rst = pin_rst;
        panel_cfg.pin_busy = -1;
        panel_cfg.panel_width = panel_width;
        panel_cfg.panel_height = panel_height;
        panel_cfg.memory_width = memory_width;
        panel_cfg.memory_height = memory_height;
        panel_cfg.offset_x = offset_x;
        panel_cfg.offset_y = offset_y;
        panel_cfg.offset_rotation = 0;
        panel_cfg.readable = true;
        panel_cfg.invert = invert_color;
        panel_cfg.rgb_order = rgb_order;
        panel_cfg.dlen_16bit = true;
        panel->config(panel_cfg);
        panel->setBus(bus);

        // Backlight
        if (bl_pin >= 0)
        {
            auto* light = new lgfx::Light_PWM();
            auto light_cfg = light->config();
            light_cfg.pin_bl = bl_pin;
            light_cfg.pwm_channel = bl_pwm_channel;
            light_cfg.invert = bl_invert;
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
    ESP_LOGI(TAG, "Display initialized (%dx%d, rotation=%d)", (int)panel_width, (int)panel_height, (int)rotation);

    // Store for static accessor
    s_LGFXDevice = device;

    // Create LovyanGFXDisplay wrapper and register with display backend
    auto display = std::make_unique<LovyanGFXDisplay>();
    if (!display->InitializeWithDevice(device, device->width(), device->height(), swap_bytes, use_psram, double_buffer))
    {
        DEKI_LOG_ERROR("LGFXDisplayPanel: Failed to initialize display wrapper");
        onComplete(false);
        return;
    }

    DekiDisplay::SetDisplay(std::move(display), "LovyanGFX");

    // Mark owner as Persistent so display persists across prefab changes
    if (GetOwner())
    {
        DekiEngine::GetInstance().GetPrefabSystem().MarkPersistent(GetOwner());
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
