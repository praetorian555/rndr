#pragma once

#include "delegate.h"
#include "input.h"
#include "opal/allocator.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/ref.h"
#include "system-message-handler.hpp"

#include "rndr/generic-window.hpp"
#include "rndr/platform-application.hpp"

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
    using WindowCloseDelegate = Delegate<bool(const GenericWindow& /*window*/)>;
    WindowCloseDelegate on_window_close;

    using WindowResizeDelegate = MultiDelegate<void(const GenericWindow& window /*window*/, int /*width*/, int /*height*/)>;
    WindowResizeDelegate on_window_resize;

    static Application* Create(const ApplicationDesc& desc = ApplicationDesc{});
    static void Destroy();

    static Application& GetChecked();
    ~Application();

    GenericWindow* CreateGenericWindow(const GenericWindowDesc& desc = GenericWindowDesc());
    void DestroyGenericWindow(GenericWindow* window);

    [[nodiscard]] struct Logger& GetLoggerChecked() const;
    [[nodiscard]] InputSystem& GetInputSystemChecked() const;

    void ProcessSystemEvents();

    void EnableHighPrecisionCursorMode(bool enable, GenericWindow& window);

    void OnWindowClose(GenericWindow& window) override;
    void OnWindowSizeChanged(const GenericWindow& window, i32 width, i32 height) override;

    bool OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnCharacter(const GenericWindow& window, uchar32 character, bool is_repeated) override;

    bool OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position) override;
    bool OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y) override;

private:
    Application(const ApplicationDesc& desc);

    template <typename T, typename... Args>
    friend T* Opal::New(Opal::AllocatorBase* /*allocator*/, Args&&... /*args*/);

    ApplicationDesc m_desc;
    Opal::Ref<PlatformApplication> m_platform_application;
    struct Logger* m_logger = nullptr;
    Opal::AllocatorBase* m_allocator = nullptr;
    InputSystem* m_input_system = nullptr;
};

}  // namespace Rndr
