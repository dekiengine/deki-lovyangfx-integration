#include "LovyanGFXTouch.h"
#include "DekiLogSystem.h"

#if defined(ESP32)

#include <LovyanGFX.hpp>
#include "LGFXDisplayPanel.h"

// millis() was provided by Arduino — use LovyanGFX's implementation instead
using lgfx::v1::millis;

LovyanGFXTouch::LovyanGFXTouch()
: gfx(nullptr)
, initialized(false)
, touch_pressed(false)
, touch_x(0)
, touch_y(0)
, last_touch_x(0)
, last_touch_y(0)
, pin_int(-1)
, stale_frame_count(0)
{
}

LovyanGFXTouch::~LovyanGFXTouch()
{
    Shutdown();
}

bool LovyanGFXTouch::Initialize()
{
    DEKI_LOG_INTERNAL("LovyanGFXTouch::Initialize() ENTRY - initialized=%d", initialized);

    if (initialized)
    {
        DEKI_LOG_INTERNAL("LovyanGFXTouch: Already initialized, returning true");
        return true;
    }

    // Get LGFX device from display panel component
    gfx = LGFXDisplayPanel::GetLGFXDevice();

    if (!gfx)
    {
        DEKI_LOG_ERROR("LovyanGFXTouch: Failed to get LovyanGFX device (LGFXDisplayPanel not setup yet?)");
        return false;
    }

    // Check if touch panel is actually available
    if (!gfx->touch())
    {
        DEKI_LOG_WARNING("LovyanGFXTouch: Touch panel not available (not configured or pin conflict)");
        // Still return true - touch is optional, and we'll safely skip in Update()
    }

    initialized = true;
    DEKI_LOG_INTERNAL("LovyanGFXTouch: Touch panel initialized successfully!");

    return true;
}

void LovyanGFXTouch::Shutdown()
{
    if (!initialized)
    {
        return;
    }

    event_callbacks.clear();
    gfx = nullptr;
    initialized = false;

    DEKI_LOG_INTERNAL("LovyanGFXTouch: Shutdown complete");
}

void LovyanGFXTouch::SetPinInt(int32_t pin)
{
    pin_int = pin;
}

void LovyanGFXTouch::Update()
{
    if (!initialized || !gfx)
    {
        return;
    }

    if (!gfx->touch())
    {
        return;
    }

    uint16_t raw_x = 0, raw_y = 0;
    uint8_t simple_count = gfx->getTouch(&raw_x, &raw_y);

    bool is_touching = (simple_count > 0);

    if (!is_touching)
    {
        stale_frame_count = 0;
        if (touch_pressed)
        {
            touch_pressed = false;

            InputEvent event;
            event.type = InputEventType::MOUSE_BUTTON_UP;
            event.x = touch_x;
            event.y = touch_y;
            event.pressed = false;
            event.timestamp = millis();

            NotifyCallbacks(event);
        }
        return;
    }

    int32_t screen_x = raw_x;
    int32_t screen_y = raw_y;

    // Software release detection when no INT pin is wired.
    // Without INT, the FT5x06 driver can report stale touch data after
    // finger lift. Detect this by checking for unchanged position.
    if (pin_int == -1 && touch_pressed)
    {
        if (screen_x == last_touch_x && screen_y == last_touch_y)
        {
            stale_frame_count++;
            if (stale_frame_count >= STALE_THRESHOLD)
            {
                touch_pressed = false;
                stale_frame_count = 0;

                InputEvent event;
                event.type = InputEventType::MOUSE_BUTTON_UP;
                event.x = touch_x;
                event.y = touch_y;
                event.pressed = false;
                event.timestamp = millis();

                NotifyCallbacks(event);
                return;
            }
        }
        else
        {
            stale_frame_count = 0;
        }
    }

    if (!touch_pressed)
    {
        touch_pressed = true;
        stale_frame_count = 0;
        touch_x = screen_x;
        touch_y = screen_y;
        last_touch_x = screen_x;
        last_touch_y = screen_y;

        InputEvent event;
        event.type = InputEventType::MOUSE_BUTTON_DOWN;
        event.x = screen_x;
        event.y = screen_y;
        event.pressed = true;
        event.timestamp = millis();

        NotifyCallbacks(event);
    }
    else
    {
        if (screen_x != last_touch_x || screen_y != last_touch_y)
        {
            touch_x = screen_x;
            touch_y = screen_y;

            InputEvent event;
            event.type = InputEventType::MOUSE_MOVE;
            event.x = screen_x;
            event.y = screen_y;
            event.timestamp = millis();

            NotifyCallbacks(event);

            last_touch_x = screen_x;
            last_touch_y = screen_y;
        }
    }
}

void LovyanGFXTouch::NotifyCallbacks(const InputEvent& event)
{
    for (const auto& callback : event_callbacks)
    {
        if (callback)
        {
            callback(event);
        }
    }
}

void LovyanGFXTouch::RegisterEventCallback(const InputEventCallback& callback)
{
    event_callbacks.push_back(callback);
    DEKI_LOG_INTERNAL("LovyanGFXTouch: Callback registered (total: %d callbacks)", event_callbacks.size());
}

bool LovyanGFXTouch::IsInitialized() const
{
    return initialized;
}

bool LovyanGFXTouch::GetPointerPosition(int32_t* x, int32_t* y) const
{
    if (x) *x = touch_x;
    if (y) *y = touch_y;
    return touch_pressed;
}

bool LovyanGFXTouch::IsKeyPressed(uint32_t key) const
{
    return false;
}

#else

// Stub implementation for non-Arduino builds
LovyanGFXTouch::LovyanGFXTouch() : initialized(false) {}
LovyanGFXTouch::~LovyanGFXTouch() {}
void LovyanGFXTouch::SetPinInt(int32_t) {}
bool LovyanGFXTouch::Initialize() { return false; }
void LovyanGFXTouch::Shutdown() {}
void LovyanGFXTouch::Update() {}
void LovyanGFXTouch::RegisterEventCallback(const InputEventCallback& callback) {}
bool LovyanGFXTouch::IsInitialized() const { return false; }
bool LovyanGFXTouch::GetPointerPosition(int32_t* x, int32_t* y) const { return false; }
bool LovyanGFXTouch::IsKeyPressed(uint32_t key) const { return false; }

#endif
