#include "rndr/platform-application.hpp"

#include "opal/allocator.h"

#if RNDR_WINDOWS
#include "rndr/platform/windows-window.hpp"
#endif

Rndr::PlatformApplication::~PlatformApplication()
{
    // Destroy windows one at a time, removing each from the list *before* it is destroyed. Window
    // teardown dispatches WM_DESTROY synchronously, which routes back through
    // GetGenericWindowByNativeHandle(); if a freed window were still listed, that lookup would read
    // freed memory (heap-use-after-free with two or more windows). Draining incrementally keeps the
    // list free of dangling entries -- the lookup for a window being torn down simply returns null.
    while (m_generic_windows.GetSize() > 0)
    {
        auto it = m_generic_windows.begin();
        Opal::ScopePtr<GenericWindow> window = std::move(*it);
        m_generic_windows.Erase(it);
        // `window` is destroyed at the end of this iteration, after it has left m_generic_windows.
    }
}

Opal::Expected<Opal::Ref<Rndr::GenericWindow>, Rndr::ErrorCode> Rndr::PlatformApplication::CreateGenericWindow(
    const GenericWindowDesc& desc)
{
    using ResultType = Opal::Expected<Opal::Ref<GenericWindow>, ErrorCode>;
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

#if RNDR_WINDOWS
    Opal::Expected<Opal::ScopePtr<GenericWindow>, ErrorCode> window_result = WindowsWindow::Create(resolved_desc);
#else
#error "Platform not supported!"
#endif
    if (!window_result.HasValue())
    {
        return ResultType(window_result.GetError());
    }
    Opal::ScopePtr<GenericWindow> window = std::move(window_result).GetValue();
    m_focused_window = window.Get();
    m_generic_windows.PushBack(std::move(window));
    return ResultType(m_focused_window.Clone());
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