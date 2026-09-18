#pragma once

#include <cstdint>
#include <vector>

#include "IDekiInput.h"  // from deki-input

// Forward declaration (must match LovyanGFX's inline namespace).
// Must stay at global scope: LovyanGFX declares ::lgfx, so declaring it inside
// DekiLovyanGfx would create a distinct, never-defined DekiLovyanGfx::lgfx.
namespace lgfx { inline namespace v1 { class LGFX_Device; } }

namespace DekiLovyanGfx
{

/**
 * @brief LovyanGFX Touch input package
 *
 * Implements DekiInput::IDekiInput interface using LovyanGFX's built-in touch support.
 * Works with any touch controller supported by LovyanGFX (XPT2046, FT5x06, etc.).
 * The touch controller is configured via the LGFXTouchPanel SetupComponent.
 */
class LovyanGFXTouch : public DekiInput::IDekiInput
{
public:
    LovyanGFXTouch();
    ~LovyanGFXTouch() override;

    // DekiInput::IDekiInput interface
    bool Initialize() override;
    void Shutdown() override;
    void Update() override;
    void RegisterEventCallback(const DekiInput::InputEventCallback& callback) override;
    bool IsInitialized() const override;
    bool GetPointerPosition(int32_t* x, int32_t* y) const override;
    bool IsKeyPressed(uint32_t key) const override;

    void SetPinInt(int32_t pin);

private:
#if defined(ESP32)
    lgfx::LGFX_Device* gfx;
    bool initialized;
    bool m_TouchPressed;
    int32_t m_TouchX;
    int32_t m_TouchY;
    int32_t m_LastTouchX;
    int32_t m_LastTouchY;
    int32_t intPin;
    int32_t m_StaleFrameCount;
    static constexpr int STALE_THRESHOLD = 3;
    std::vector<DekiInput::InputEventCallback> m_EventCallbacks;

    void NotifyCallbacks(const DekiInput::InputEvent& event);
#else
    bool initialized = false;
#endif
};

}  // namespace DekiLovyanGfx
