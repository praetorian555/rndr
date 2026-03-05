#include "rndr/platform-application.hpp"

#include "opal/allocator.h"

#if RNDR_WINDOWS
#include "rndr/platform/windows-window.hpp"
#endif

Opal::Ref<Rndr::GenericWindow> Rndr::PlatformApplication::CreateGenericWindow(const GenericWindowDesc& desc)
{
    Opal::ScopePtr<GenericWindow> window;
#if RNDR_WINDOWS
    window = Opal::MakeScoped<GenericWindow, WindowsWindow>(Opal::GetDefaultAllocator(), desc);
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