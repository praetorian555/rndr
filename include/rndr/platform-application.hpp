#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/scope-ptr.h"

#include "rndr/generic-window.hpp"
#include "rndr/math.hpp"
#include "rndr/monitor-info.hpp"
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
    virtual ~PlatformApplication();

    Opal::Ref<GenericWindow> CreateGenericWindow(const GenericWindowDesc& desc);
    void DestroyGenericWindow(Opal::Ref<GenericWindow> window);

    /**
     * Process any messages received from the OS, like input events.
     * @param timeout_ms How long to block waiting for an event before returning. Use 0 to return immediately
     *                  if no events are pending, or InfiniteTimeout to block until at least one event arrives.
     */
    virtual void ProcessSystemEvents(u32 timeout_ms) = 0;

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

    /**
     * Check if a gamepad is connected on the given slot.
     * @param gamepad_index Slot in [0, k_max_gamepads).
     */
    [[nodiscard]] virtual bool IsGamepadConnected(u8 gamepad_index) const
    {
        (void)gamepad_index;
        return false;
    }

    [[nodiscard]] virtual Opal::DynamicArray<MonitorInfo> GetMonitors() const = 0;
    [[nodiscard]] virtual MonitorInfo GetPrimaryMonitor() const = 0;
    [[nodiscard]] virtual MonitorInfo GetMonitorAtPosition(const Vector2i& pos) const = 0;
    [[nodiscard]] virtual MonitorInfo GetMonitorForWindow(const GenericWindow& window) const = 0;

    Opal::Ref<class GenericWindow> GetGenericWindowByNativeHandle(NativeWindowHandle handle);
    [[nodiscard]] const ModifierKeysState& GetModifierKeysState() const { return m_modifier_keys; }

protected:
    struct SystemMessageHandler* m_message_handler;
    Opal::DynamicArray<Opal::ScopePtr<GenericWindow>> m_generic_windows;
    Opal::Ref<GenericWindow> m_focused_window;
    ModifierKeysState m_modifier_keys;
};

}  // namespace Rndr
