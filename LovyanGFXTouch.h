#pragma once

#include <cstdint>
#include <vector>

#include "providers/IDekiInput.h"

// Forward declaration (must match LovyanGFX's inline namespace)
namespace lgfx { inline namespace v1 { class LGFX_Device; } }

/**
 * @brief LovyanGFX Touch input module
 *
 * Implements IDekiInput interface using LovyanGFX's built-in touch support.
 * Works with any touch controller supported by LovyanGFX (XPT2046, FT5x06, etc.).
 * The touch controller is configured via the LGFXTouchPanel SetupComponent.
 */
class LovyanGFXTouch : public IDekiInput
{
public:
    LovyanGFXTouch();
    ~LovyanGFXTouch() override;

    // IDekiInput interface
    bool Initialize() override;
    void Shutdown() override;
    void Update() override;
    void RegisterEventCallback(const InputEventCallback& callback) override;
    bool IsInitialized() const override;
    bool GetPointerPosition(int32_t* x, int32_t* y) const override;
    bool IsKeyPressed(uint32_t key) const override;

    void SetPinInt(int32_t pin);

private:
#if defined(ESP32)
    lgfx::LGFX_Device* gfx;
    bool initialized;
    bool touch_pressed;
    int32_t touch_x;
    int32_t touch_y;
    int32_t last_touch_x;
    int32_t last_touch_y;
    int32_t pin_int;
    int32_t stale_frame_count;
    static constexpr int STALE_THRESHOLD = 3;
    std::vector<InputEventCallback> event_callbacks;

    void NotifyCallbacks(const InputEvent& event);
#else
    bool initialized = false;
#endif
};
