#include "rndr/platform-application.hpp"

#include "opal/allocator.h"

#if RNDR_WINDOWS
#include "rndr/platform/windows-window.hpp"
#endif

Opal::Ref<Rndr::GenericWindow> Rndr::PlatformApplication::CreateGenericWindow(const GenericWindowDesc& desc)
{
    GenericWindowDesc resolved_desc = desc;
    if (resolved_desc.monitor_index >= 0)
    {
        Opal::DynamicArray<MonitorInfo> monitors = GetMonitors();
        if (resolved_desc.monitor_index < monitors.GetSize())
        {
            const MonitorInfo& monitor = monitors[resolved_desc.monitor_index];
            resolved_desc.start_x = monitor.position.x + (monitor.size.x - resolved_desc.width) / 2;
            resolved_desc.start_y = monitor.position.y + (monitor.size.y - resolved_desc.height) / 2;
        }
    }

    Opal::ScopePtr<GenericWindow> window;
#if RNDR_WINDOWS
    window = Opal::MakeScoped<GenericWindow, WindowsWindow>(Opal::GetDefaultAllocator(), resolved_desc);
#else
#error "Platform not supported!"
#endif
    m_focused_window = window.Get();
    if (window != nullptr)
    {
        m_generic_windows.PushBack(std::move(window));
    }
    return m_focused_window.Get();
}

void Rndr::PlatformApplication::DestroyGenericWindow(Opal::Ref<GenericWindow> window)
{
    for (auto it = m_generic_windows.begin(); it != m_generic_windows.end(); ++it)
    {
        if (window.GetPtr() == it->Get())
        {
            m_generic_windows.Erase(it);
            break;
        }
    }
}

Opal::Ref<Rndr::GenericWindow> Rndr::PlatformApplication::GetGenericWindowByNativeHandle(NativeWindowHandle handle)
{
    for (const auto& window : m_generic_windows)
    {
        if (window->GetNativeHandle() == handle)
        {
            return window.Get();
        }
    }
    return nullptr;
}