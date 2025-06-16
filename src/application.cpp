#include "rndr/application.hpp"

#include <stdio.h>

#include "opal/container/in-place-array.h"
#include "rndr/input.h"

#include "rndr/log.h"

#if RNDR_WINDOWS
#include "rndr/platform/windows-application.hpp"
#include "rndr/platform/windows-window.hpp"
#endif

namespace
{
Rndr::StdLogger g_default_logger;
Rndr::Application* g_instance = nullptr;
}  // namespace

Rndr::Application* Rndr::Application::Create(const ApplicationDesc& desc)
{
    if (g_instance == nullptr)
    {
        g_instance = Opal::New<Application>(desc.user_allocator, desc);
        return g_instance;
    }
    RNDR_ASSERT(false, "There can only be one instance of the Rndr::Application!");
    return nullptr;
}

void Rndr::Application::Destroy()
{
    if (g_instance != nullptr)
    {
        Opal::Delete(g_instance->m_allocator, g_instance);
        g_instance = nullptr;
    }
}

Rndr::Application& Rndr::Application::GetChecked()
{
    RNDR_ASSERT(g_instance != nullptr, "There is no instance of the Rndr::Application!");
    return *g_instance;
}

Rndr::Application::Application(const ApplicationDesc& desc)
    : m_desc(desc),
      m_logger(desc.user_logger != nullptr ? desc.user_logger : &g_default_logger),
      m_allocator(desc.user_allocator != nullptr ? desc.user_allocator : Opal::GetDefaultAllocator())
{
#if RNDR_WINDOWS
    m_platform_application = Opal::New<WindowsApplication>(m_allocator, this, m_allocator);
#else
#error "Platform not supported!"
#endif

    m_input_system = InputSystem::Get();
    if (desc.enable_input_system && !m_input_system->Init())
    {
        RNDR_LOG_ERROR("Failed to initialize the input system!");
        return;
    }
}

Rndr::Application::~Application()
{
    if (m_desc.enable_input_system)
    {
        InputSystem& input_system = InputSystem::GetChecked();
        input_system.Destroy();
    }
    if (m_platform_application.IsValid())
    {
        Opal::Delete(m_allocator, m_platform_application.GetPtr());
    }
}

Rndr::GenericWindow* Rndr::Application::CreateGenericWindow(const GenericWindowDesc& desc)
{
    return m_platform_application->CreateGenericWindow(desc);
}

void Rndr::Application::DestroyGenericWindow(GenericWindow* window)
{
    m_platform_application->DestroyGenericWindow(window);
}

Rndr::Logger& Rndr::Application::GetLoggerChecked() const
{
    RNDR_ASSERT(m_logger != nullptr, "There is no logger!");
    return *m_logger;
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
        m_input_system->ProcessEvents(delta_seconds);
    }
}

void Rndr::Application::EnableHighPrecisionCursorMode(bool enable, GenericWindow& window)
{
    m_platform_application->EnableHighPrecisionCursorMode(enable, window);
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
    window.ForceClose();
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

bool Rndr::Application::OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated)
{
    // RNDR_LOG_DEBUG("ButtonDown Key=0x%x, IsRepeated=%s", key_code, is_repeated ? "true" : "false");
    m_input_system->OnButtonDown(window, key_code, is_repeated);
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnButtonDown(window, key_code, is_repeated);
    }
    return true;
}

bool Rndr::Application::OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated)
{
    // RNDR_LOG_DEBUG("ButtonUp Key=0x%x, IsRepeated=%s", key_code, is_repeated ? "true" : "false");
    m_input_system->OnButtonUp(window, key_code, is_repeated);
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnButtonUp(window, key_code, is_repeated);
    }
    return true;
}

bool Rndr::Application::OnCharacter(const GenericWindow& window, uchar32 character, bool is_repeated)
{
    // Opal::StringUtf32 in;
    // in.Append(character);
    // Opal::StringUtf8 out(10, 0);
    // Opal::Transcode(in, out);
    // RNDR_LOG_DEBUG("Character Char=%s, IsRepeated=%s", out.GetData(), is_repeated ? "true" : "false");
    m_input_system->OnCharacter(window, character, is_repeated);
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnCharacter(window, character, is_repeated);
    }
    return true;
}

bool Rndr::Application::OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    // RNDR_LOG_DEBUG("MouseButtonDown Key=0x%x, CursorPosition=(x=%d, y=%d)", primitive, cursor_position.x, cursor_position.y);
    m_input_system->OnMouseButtonDown(window, primitive, cursor_position);
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMouseButtonDown(window, primitive, cursor_position);
    }
    return true;
}

bool Rndr::Application::OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    // RNDR_LOG_DEBUG("MouseButtonUp Key=0x%x, CursorPosition=(x=%d, y=%d)", primitive, cursor_position.x, cursor_position.y);
    m_input_system->OnMouseButtonUp(window, primitive, cursor_position);
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMouseButtonUp(window, primitive, cursor_position);
    }
    return true;
}

bool Rndr::Application::OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    // RNDR_LOG_DEBUG("MouseDoubleClick Key=0x%x, CursorPosition=(x=%d, y=%d)", primitive, cursor_position.x, cursor_position.y);
    m_input_system->OnMouseDoubleClick(window, primitive, cursor_position);
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMouseDoubleClick(window, primitive, cursor_position);
    }
    return true;
}

bool Rndr::Application::OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position)
{
    // RNDR_LOG_DEBUG("MouseWheel Delta=%f, CursorPosition=(x=%d, y=%d)", wheel_delta, cursor_position.x, cursor_position.y);
    m_input_system->OnMouseWheel(window, wheel_delta, cursor_position);
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMouseWheel(window, wheel_delta, cursor_position);
    }
    return true;
}

bool Rndr::Application::OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y)
{
    m_input_system->OnMouseMove(window, delta_x, delta_y);
    for (const Opal::Ref<SystemMessageHandler>& system_message_handler : m_system_message_handlers)
    {
        system_message_handler->OnMouseMove(window, delta_x, delta_y);
    }
    return true;
}
