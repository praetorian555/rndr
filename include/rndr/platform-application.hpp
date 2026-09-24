#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/scope-ptr.h"

#include "rndr/error-codes.hpp"
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

    Opal::Expected<Opal::Ref<GenericWindow>, ErrorCode> CreateGenericWindow(const GenericWindowDesc& desc);
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

    /**
     * Put text on the system clipboard, replacing whatever was there.
     * @param text UTF-8 text.
     * @return ErrorCode::Success, ErrorCode::InvalidArgument if the text is not valid UTF-8, or
     *         ErrorCode::PlatformError if the window system would not take it.
     */
    virtual ErrorCode SetClipboardText(const Opal::StringUtf8& text) = 0;

    /**
     * Get the text on the system clipboard. An empty clipboard, or one holding something that is not
     * text, gives an empty string. On Linux, when another application owns the clipboard, this waits
     * for it to send the text over, for up to a second.
     * @return The text, ErrorCode::CorruptData if what the clipboard holds is not valid text in its
     *         claimed encoding, or ErrorCode::PlatformError if the window system could not deliver it.
     */
    [[nodiscard]] virtual Opal::Expected<Opal::StringUtf8, ErrorCode> GetClipboardText() = 0;

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
