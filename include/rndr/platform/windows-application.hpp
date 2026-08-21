#pragma once

#include "opal/container/dynamic-array.h"

#include "rndr/definitions.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/input-primitives.hpp"
#include "rndr/platform-application.hpp"
#include "rndr/platform/windows-gamepad.hpp"
#include "rndr/time.hpp"

#if RNDR_WINDOWS
#include "rndr/platform/windows-forward-def.hpp"
#endif

namespace Rndr
{

struct WindowsDeferredMessage
{
    class WindowsWindow* window;
    UINT code;
    WPARAM param_w;
    LPARAM param_l;
};

class WindowsApplication : public PlatformApplication
{
public:
    WindowsApplication(struct SystemMessageHandler* message_handler);

    i32 ProcessMessage(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l);

    void ProcessSystemEvents(u32 timeout_ms) override;

    void ShowCursor(bool show) override;
    [[nodiscard]] bool IsCursorVisible() const override;
    void SetCursorPosition(const Vector2i& pos) override;
    [[nodiscard]] Vector2i GetCursorPosition() const override;

    [[nodiscard]] bool IsGamepadConnected(u8 gamepad_index) const override;

    [[nodiscard]] Opal::DynamicArray<MonitorInfo> GetMonitors() const override;
    [[nodiscard]] MonitorInfo GetPrimaryMonitor() const override;
    [[nodiscard]] MonitorInfo GetMonitorAtPosition(const Vector2i& pos) const override;
    [[nodiscard]] MonitorInfo GetMonitorForWindow(const GenericWindow& window) const override;

private:
    i32 TranslateKey(i32 win_key, i32 desc);
    bool GetInputPrimitive(InputPrimitive& out_primitive, i32 virtual_key);

    /** Samples every gamepad slot. Called from ProcessSystemEvents. */
    void PollGamepads();

    WindowsGamepad m_gamepads[k_max_gamepads];

    // XInput has no message queue to drain, so gamepads are polled instead. ProcessSystemEvents
    // takes no delta time, so the poll interval is measured here rather than passed in.
    Timestamp m_last_gamepad_poll_timestamp = 0;
};

}  // namespace Rndr

namespace RndrPrivate
{
#if RNDR_WINDOWS
LRESULT CALLBACK WindowProc(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l);
#endif
}  // namespace RndrPrivate
