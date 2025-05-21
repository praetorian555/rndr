#include "rndr/platform-application.hpp"

#if RNDR_WINDOWS
#include "rndr/platform/windows-window.hpp"
#endif

Rndr::GenericWindow* Rndr::PlatformApplication::CreateGenericWindow(const GenericWindowDesc& desc)
{
    GenericWindow* window = nullptr;
#if RNDR_WINDOWS
    window = Opal::New<WindowsWindow>(m_allocator, desc, m_allocator);
#else
#error "Platform not supported!"
#endif
    if (window != nullptr)
    {
        m_generic_windows.PushBack(window);
    }
    return window;
}

void Rndr::PlatformApplication::DestroyGenericWindow(GenericWindow* window)
{
    for (auto it = m_generic_windows.begin(); it != m_generic_windows.end(); ++it)
    {
        if (window == *it)
        {
            m_generic_windows.Erase(it);
            break;
        }
    }
    window->Destroy();
}

Rndr::GenericWindow* Rndr::PlatformApplication::GetGenericWindowByNativeHandle(NativeWindowHandle handle)
{
    for (auto* window : m_generic_windows)
    {
        if (window->GetNativeHandle() == handle)
        {
            return window;
        }
    }
    return nullptr;
}