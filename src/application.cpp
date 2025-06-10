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

    if (desc.enable_input_system && !InputSystem::Init())
    {
        RNDR_LOG_ERROR("Failed to initialize the input system!");
        return;
    }
}

Rndr::Application::~Application()
{
    if (m_desc.enable_input_system)
    {
        InputSystem::Destroy();
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

void Rndr::Application::ProcessSystemEvents()
{
    m_platform_application->ProcessSystemEvents();
}

void Rndr::Application::OnWindowClose(GenericWindow* window)
{
    const bool is_handled = on_window_close.Execute(window);
    if (!is_handled)
    {
        window->ForceClose();
    }
}

void Rndr::Application::OnWindowSizeChanged(GenericWindow* window, i32 width, i32 height)
{
    on_window_resize.Execute(window, width, height);
}

bool Rndr::Application::OnButtonDown(InputPrimitive key_code, bool is_repeated)
{
    RNDR_LOG_DEBUG("ButtonDown Key=0x%x, IsRepeated=%s", key_code, is_repeated ? "true" : "false");
    return true;
}

bool Rndr::Application::OnButtonUp(InputPrimitive key_code, bool is_repeated)
{
    RNDR_LOG_DEBUG("ButtonUp Key=0x%x, IsRepeated=%s", key_code, is_repeated ? "true" : "false");
    return true;
}

bool Rndr::Application::OnCharacter(uchar32 character, bool is_repeated)
{
    Opal::StringUtf32 in;
    in.Append(character);
    Opal::StringUtf8 out(10, 0);
    Opal::Transcode(in, out);
    RNDR_LOG_DEBUG("Character Char=%s, IsRepeated=%s", out.GetData(), is_repeated ? "true" : "false");
    return true;
}
