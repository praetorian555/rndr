#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/scope-ptr.h"

#include "rndr/generic-window.hpp"
#include "rndr/math.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

struct ModifierKeysState
{
    bool is_left_shift_down = false;
    bool is_right_shift_down = false;
    bool is_left_control_down = false;
    bool is_right_control_down = false;
    bool is_left_alt_down = false;
    bool is_right_alt_down = false;
    bool is_left_command_down = false;
    bool is_right_command_down = false;
    bool is_caps_locked = false;
};

class PlatformApplication
{
public:
    PlatformApplication(struct SystemMessageHandler* message_handler) : m_message_handler(message_handler) {}
    virtual ~PlatformApplication() = default;

    Opal::Ref<GenericWindow> CreateGenericWindow(const GenericWindowDesc& desc);
    void DestroyGenericWindow(Opal::Ref<GenericWindow> window);

    /**
     * Process any messages received from the OS in the previous frame, like input events.
     */
    virtual void ProcessSystemEvents() = 0;

    /**
     * Control cursor visibility.
     * @param show Should the cursor be shown or not.
     */
    virtual void ShowCursor(bool show) = 0;

    /**
     * Check if the cursor is visible.
     * @return Returns true if the cursor is visible, false otherwise.
     */
    [[nodiscard]] virtual bool IsCursorVisible() const = 0;

    /**
     * Set cursor position in screen space.
     * @param pos New cursor position.
     */
    virtual void SetCursorPosition(const Vector2i& pos) = 0;

    /**
     * Get the current cursor position.
     */
    [[nodiscard]] virtual Vector2i GetCursorPosition() const = 0;

    Opal::Ref<class GenericWindow> GetGenericWindowByNativeHandle(NativeWindowHandle handle);
    [[nodiscard]] const ModifierKeysState& GetModifierKeysState() const { return m_modifier_keys; }

protected:
    struct SystemMessageHandler* m_message_handler;
    Opal::DynamicArray<Opal::ScopePtr<GenericWindow>> m_generic_windows;
    Opal::Ref<GenericWindow> m_focused_window;
    ModifierKeysState m_modifier_keys;
};

}  // namespace Rndr
