#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/ref.h"
#include "opal/container/scope-ptr.h"
#include "opal/delegate.h"

#include "rndr/error-codes.hpp"
#include "rndr/monitor-info.hpp"
#include "rndr/system-message-handler.hpp"

namespace Rndr
{

struct ApplicationDesc
{
    /** If we should enable the input system. Defaults to no. */
    bool enable_input_system = false;
};

class GenericWindow;
struct GenericWindowDesc;

class Application : public SystemMessageHandler
{
public:
    using WindowCloseDelegate = Opal::Delegate<bool(const GenericWindow& /*window*/)>;
    WindowCloseDelegate on_window_close;

    using WindowResizeDelegate = Opal::MultiDelegate<void(const GenericWindow& window /*window*/, int /*width*/, int /*height*/)>;
    WindowResizeDelegate on_window_resize;

    using MonitorChangeDelegate = Opal::MultiDelegate<void()>;
    MonitorChangeDelegate on_monitor_change;

    using WindowDpiChangeDelegate = Opal::MultiDelegate<void(const GenericWindow& /*window*/, f32 /*new_dpi_scale*/)>;
    WindowDpiChangeDelegate on_window_dpi_change;

    using GamepadConnectionDelegate = Opal::MultiDelegate<void(u8 /*gamepad_index*/, bool /*is_connected*/)>;
    GamepadConnectionDelegate on_gamepad_connection_change;

    /**
     * Creates the one Application instance. Reports ErrorCode::InvalidArgument when one already exists.
     */
    [[nodiscard]] static Opal::Expected<Opal::ScopePtr<Application>, ErrorCode> Create(const ApplicationDesc& desc = ApplicationDesc{});

    static Application* Get();
    static Application& GetChecked();
    ~Application() override;

    Opal::Expected<Opal::Ref<GenericWindow>, ErrorCode> CreateGenericWindow(const GenericWindowDesc& desc);
    void DestroyGenericWindow(Opal::Ref<GenericWindow> window);

    [[nodiscard]] class InputSystem& GetInputSystemChecked() const;

    /** Monitor API. */
    [[nodiscard]] Opal::DynamicArray<MonitorInfo> GetMonitors() const;
    [[nodiscard]] MonitorInfo GetPrimaryMonitor() const;
    [[nodiscard]] MonitorInfo GetMonitorAtPosition(const Vector2i& pos) const;
    [[nodiscard]] MonitorInfo GetMonitorForWindow(const GenericWindow& window) const;
    /** End of monitor API. */

    /** Sentinel timeout value passed to ProcessSystemEvents that causes it to block until an event arrives. */
    static constexpr u32 k_infinite_timeout = 0xFFFFFFFFu;

    /**
     * Process any messages received from the OS, like input events.
     * @param timeout_ms How long to block waiting for an event before returning. Defaults to 0 which returns
     *                  immediately if no events are pending. Pass k_infinite_timeout to block until at least
     *                  one event arrives.
     */
    void ProcessSystemEvents(u32 timeout_ms = 0);

    /** Cursor manipulation API. */
    void ShowCursor(bool show);
    [[nodiscard]] bool IsCursorVisible() const;
    void SetCursorPosition(const Vector2i& pos);
    [[nodiscard]] Vector2i GetCursorPosition() const;
    /** End of cursor manipulation API. */

    /**
     * Checks whether a gamepad is currently connected on the given slot. Connection state is
     * refreshed by ProcessSystemEvents.
     * @param gamepad_index Slot in [0, k_max_gamepads).
     */
    [[nodiscard]] bool IsGamepadConnected(u8 gamepad_index) const;

    void RegisterSystemMessageHandler(SystemMessageHandler* handler);
    void UnregisterSystemMessageHandler(SystemMessageHandler* handler);

    /** Implementation of SystemMessageHandler API */
    bool OnWindowClose(GenericWindow& window) override;
    void OnWindowSizeChanged(const GenericWindow& window, i32 width, i32 height) override;
    void OnMonitorChange() override;
    void OnWindowDpiChanged(const GenericWindow& window, f32 new_dpi_scale) override;

    bool OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnCharacter(const GenericWindow& window, uchar32 character, bool is_repeated) override;

    bool OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position) override;
    bool OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y, const Vector2i& cursor_position) override;

    bool OnGamepadButtonDown(u8 gamepad_index, GamepadButton button) override;
    bool OnGamepadButtonUp(u8 gamepad_index, GamepadButton button) override;
    bool OnGamepadAxis(u8 gamepad_index, GamepadAxis axis, f32 value) override;
    void OnGamepadConnectionChanged(u8 gamepad_index, bool is_connected) override;
    /** End of SystemMessageHandler API */

private:
    explicit Application(const ApplicationDesc& desc);

    template <typename T, typename... Args>
    friend T* Opal::New(Opal::AllocatorBase* /*allocator*/, Args&&... /*args*/);

    ApplicationDesc m_desc;
    Opal::ScopePtr<class PlatformApplication> m_platform_application;
    Opal::ScopePtr<class InputSystem> m_input_system;
    Opal::DynamicArray<Opal::Ref<SystemMessageHandler>> m_system_message_handlers;
};

}  // namespace Rndr
