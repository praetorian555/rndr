#pragma once

#include "opal/allocator.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/ref.h"
#include "opal/container/scope-ptr.h"
#include "opal/delegate.h"

#include "rndr/generic-window.hpp"
#include "rndr/imgui-system.hpp"
#include "rndr/input-system.hpp"
#include "rndr/platform-application.hpp"
#include "rndr/system-message-handler.hpp"

namespace Opal
{
struct AllocatorBase;
}

namespace Rndr
{

struct ApplicationDesc
{
    /** User specified logger. User is responsible for keeping it alive and deallocating it. */
    struct Logger* user_logger;

    Opal::AllocatorBase* user_allocator;

    /** If we should enable the input system. Defaults to no. */
    bool enable_input_system = false;

    /** If we should enable the CPU tracer. Defaults to no. */
    bool enable_cpu_tracer = false;
};

class Application : public SystemMessageHandler
{
public:
    using WindowCloseDelegate = Opal::Delegate<bool(const GenericWindow& /*window*/)>;
    WindowCloseDelegate on_window_close;

    using WindowResizeDelegate = Opal::MultiDelegate<void(const GenericWindow& window /*window*/, int /*width*/, int /*height*/)>;
    WindowResizeDelegate on_window_resize;

    static Opal::ScopePtr<Application> Create(const ApplicationDesc& desc = ApplicationDesc{});

    static Application* Get();
    static Application& GetChecked();
    ~Application() override;

    GenericWindow* CreateGenericWindow(const GenericWindowDesc& desc = GenericWindowDesc());
    void DestroyGenericWindow(GenericWindow* window);

    [[nodiscard]] struct Logger& GetLoggerChecked() const;
    [[nodiscard]] InputSystem& GetInputSystemChecked() const;

    void ProcessSystemEvents(f32 delta_seconds);

    /** Cursor manipulation API. */
    void EnableHighPrecisionCursorMode(bool enable, GenericWindow& window);
    void ShowCursor(bool show);
    [[nodiscard]] bool IsCursorVisible() const;
    void SetCursorPosition(const Vector2i& pos);
    [[nodiscard]] Vector2i GetCursorPosition() const;
    void SetCursorPositionMode(CursorPositionMode mode);
    [[nodiscard]] CursorPositionMode GetCursorPositionMode() const;
    /** End of cursor manipulation API. */

    void RegisterSystemMessageHandler(SystemMessageHandler* handler);
    void UnregisterSystemMessageHandler(SystemMessageHandler* handler);

    /** Implementation of SystemMessageHandler API */
    bool OnWindowClose(GenericWindow& window) override;
    void OnWindowSizeChanged(const GenericWindow& window, i32 width, i32 height) override;

    bool OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnCharacter(const GenericWindow& window, uchar32 character, bool is_repeated) override;

    bool OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position) override;
    bool OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y) override;
    /** End of SystemMessageHandler API */

private:
    explicit Application(const ApplicationDesc& desc);

    template <typename T, typename... Args>
    friend T* Opal::New(Opal::AllocatorBase* /*allocator*/, Args&&... /*args*/);

    ApplicationDesc m_desc;
    Opal::Ref<PlatformApplication> m_platform_application;
    struct Logger* m_logger = nullptr;
    Opal::AllocatorBase* m_allocator = nullptr;
#if RNDR_OLD_INPUT_SYSTEM
    InputSystem* m_input_system = nullptr;
#else
    Opal::ScopePtr<InputSystem> m_input_system;
#endif
    Opal::Ref<ImGuiContext> m_imgui_system;
    Opal::DynamicArray<Opal::Ref<SystemMessageHandler>> m_system_message_handlers;
};

}  // namespace Rndr
