#include "rndr/application.hpp"

#include "opal/allocator.h"
#include "opal/container/in-place-array.h"
#include "opal/container/scope-ptr.h"
#include "opal/logging.h"

#include "rndr/generic-window.hpp"
#include "rndr/input-system.hpp"
#include "rndr/log.hpp"
#include "rndr/monitor-info.hpp"
#include "rndr/platform-application.hpp"

#if RNDR_WINDOWS
#include "rndr/platform/windows-application.hpp"
#include "rndr/platform/windows-window.hpp"
#endif

namespace
{
Rndr::Application* g_instance = nullptr;
}  // namespace

Opal::ScopePtr<Rndr::Application> Rndr::Application::Create(const ApplicationDesc& desc)
{
    if (g_instance != nullptr)
    {
        throw Opal::Exception("Rndr Application already created!");
    }
    Opal::ScopePtr<Application> app = Opal::MakeScoped<Application>(nullptr, desc);
    g_instance = app.Get();
    return app;
}

Rndr::Application* Rndr::Application::Get()
{
    return g_instance;
}

Rndr::Application& Rndr::Application::GetChecked()
{
    RNDR_ASSERT(g_instance != nullptr, "There is no instance of the Rndr::Application!");
    return *g_instance;
}

Rndr::Application::Application(const ApplicationDesc& desc) : m_desc(desc)
{
    Opal::Logger& logger = Opal::GetLogger();
    if (!logger.IsCategoryRegistered("Rndr"))
    {
        logger.RegisterCategory("Rndr", Opal::LogLevel::Verbose);
    }
#if RNDR_WINDOWS
    m_platform_application = Opal::MakeScoped<PlatformApplication, WindowsApplication>(Opal::GetDefaultAllocator(), this);
#else
#error "Platform not supported!"
#endif

    if (m_desc.enable_input_system)
    {
        m_input_system = Opal::MakeScoped<InputSystem>(nullptr);
        RegisterSystemMessageHandler(m_input_system.Get());
    }
}

Rndr::Application::~Application()
{
    g_instance = nullptr;
}

Opal::Ref<Rndr::GenericWindow> Rndr::Application::CreateGenericWindow(const GenericWindowDesc& desc)
{
    return m_platform_application->CreateGenericWindow(desc);
}

void Rndr::Application::DestroyGenericWindow(Opal::Ref<GenericWindow> window)
{
    m_platform_application->DestroyGenericWindow(std::move(window));
}

Rndr::InputSystem& Rndr::Application::GetInputSystemChecked() const
{
    RNDR_ASSERT(m_desc.enable_input_system, "Input system not enabled!");
    return *m_input_system;
}

void Rndr::Application::ProcessSystemEvents(f32 delta_seconds)
{
    m_platform_application->ProcessSystemEvents();
    if (m_desc.enable_input_system)
    {
        m_input_system->ProcessSystemEvents(delta_seconds);
    }
}

void Rndr::Application::ShowCursor(bool show)
{
    m_platform_application->ShowCursor(show);
}

bool Rndr::Application::IsCursorVisible() const
{
    return m_platform_application->IsCursorVisible();
}

void Rndr::Application::SetCursorPosition(const Vector2i& pos)
{
    m_platform_application->SetCursorPosition(pos);
}

Rndr::Vector2i Rndr::Application::GetCursorPosition() const
{
    return m_platform_application->GetCursorPosition();
}

Opal::DynamicArray<Rndr::MonitorInfo> Rndr::Application::GetMonitors() const
{
    return m_platform_application->GetMonitors();
}

Rndr::MonitorInfo Rndr::Application::GetPrimaryMonitor() const
{
    return m_platform_application->GetPrimaryMonitor();
}

Rndr::MonitorInfo Rndr::Application::GetMonitorAtPosition(const Vector2i& pos) const
{
    return m_platform_application->GetMonitorAtPosition(pos);
}

Rndr::MonitorInfo Rndr::Application::GetMonitorForWindow(const GenericWindow& window) const
{
    return m_platform_application->GetMonitorForWindow(window);
}

void Rndr::Application::RegisterSystemMessageHandler(SystemMessageHandler* handler)
{
    if (handler == nullptr)
    {
        RNDR_LOG_ERROR("Trying to register invalid system message handler!");
        return;
    }
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        if (system_message_handler.GetPtr() == handler)
        {
            return;
        }
    }
    m_system_message_handlers.PushBack(Opal::Ref(*handler));
}

void Rndr::Application::UnregisterSystemMessageHandler(SystemMessageHandler* handler)
{
    for (i32 i = 0; i < m_system_message_handlers.GetSize(); i++)
    {
        const Opal::Ref<SystemMessageHandler>& system_message_handler = m_system_message_handlers[i];
        if (system_message_handler.GetPtr() == handler)
        {
            m_system_message_handlers.EraseWithSwap(m_system_message_handlers.begin() + i);
            return;
        }
    }
}

bool Rndr::Application::OnWindowClose(GenericWindow& window)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        if (system_message_handler->OnWindowClose(window))
        {
            return true;
        }
    }
    if (on_window_close.Execute(window))
    {
        return true;
    }
    window.MarkClosed();
    return true;
}

void Rndr::Application::OnWindowSizeChanged(const GenericWindow& window, i32 width, i32 height)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnWindowSizeChanged(window, width, height);
    }
    on_window_resize.Execute(window, width, height);
}

void Rndr::Application::OnMonitorChange()
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMonitorChange();
    }
    on_monitor_change.Execute();
}

void Rndr::Application::OnWindowDpiChanged(const GenericWindow& window, f32 new_dpi_scale)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnWindowDpiChanged(window, new_dpi_scale);
    }
    on_window_dpi_change.Execute(window, new_dpi_scale);
}

bool Rndr::Application::OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnButtonDown(window, key_code, is_repeated);
    }
    return true;
}

bool Rndr::Application::OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnButtonUp(window, key_code, is_repeated);
    }
    return true;
}

bool Rndr::Application::OnCharacter(const GenericWindow& window, uchar32 character, bool is_repeated)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnCharacter(window, character, is_repeated);
    }
    return true;
}

bool Rndr::Application::OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMouseButtonDown(window, primitive, cursor_position);
    }
    return true;
}

bool Rndr::Application::OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMouseButtonUp(window, primitive, cursor_position);
    }
    return true;
}

bool Rndr::Application::OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMouseDoubleClick(window, primitive, cursor_position);
    }
    return true;
}

bool Rndr::Application::OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMouseWheel(window, wheel_delta, cursor_position);
    }
    return true;
}

bool Rndr::Application::OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y)
{
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMouseMove(window, delta_x, delta_y);
    }
    return true;
}
