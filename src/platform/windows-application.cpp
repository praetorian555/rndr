#include "rndr/platform/windows-application.hpp"

#include "glad/glad_wgl.h"

#include "opal/container/in-place-array.h"

#include "rndr/application.hpp"
#include "rndr/platform/windows-window.hpp"
#include "rndr/system-message-handler.hpp"

Rndr::WindowsApplication* g_windows_app = nullptr;

Rndr::WindowsApplication::WindowsApplication(SystemMessageHandler* message_handler, Opal::AllocatorBase* allocator)
    : PlatformApplication(message_handler, allocator), m_deferred_messages(m_allocator)
{
    g_windows_app = this;
    // TODO: Iterate over connected devices using raw input API
}

LRESULT RndrPrivate::WindowProc(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l)
{
    return g_windows_app->ProcessMessage(window_handle, msg_code, param_w, param_l);
}

Rndr::i32 Rndr::WindowsApplication::ProcessMessage(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l)
{
    GenericWindow* window = GetGenericWindowByNativeHandle(reinterpret_cast<NativeWindowHandle>(window_handle));
    if (window == nullptr)
    {
        return static_cast<i32>(DefWindowProc(window_handle, msg_code, param_w, param_l));
    }
    switch (msg_code)
    {
        case WM_CLOSE:
        case WM_SIZE:
        {
            DeferMessage(static_cast<WindowsWindow*>(window), msg_code, param_w, param_l);
            return 0;
        }
    }
    return static_cast<i32>(DefWindowProc(window_handle, msg_code, param_w, param_l));
}

void Rndr::WindowsApplication::ProcessSystemEvents()
{
    for (auto* window : m_generic_windows)
    {
        MSG msg;
        while (PeekMessage(&msg, reinterpret_cast<HWND>(window->GetNativeHandle()), 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

void Rndr::WindowsApplication::DeferMessage(WindowsWindow* window, UINT msg_code, WPARAM param_w, LPARAM param_l)
{
    m_deferred_messages.PushBack(WindowsDeferredMessage{window, msg_code, param_w, param_l});
}

void Rndr::WindowsApplication::ProcessDeferredMessages(f32 delta_seconds)
{
    for (WindowsDeferredMessage& message : m_deferred_messages)
    {
        ProcessDeferredMessage(message, delta_seconds);
    }
    m_deferred_messages.Clear();
}

void Rndr::WindowsApplication::ProcessDeferredMessage(WindowsDeferredMessage& msg, f32)
{
    switch (msg.code)
    {
        case WM_CLOSE:
        {
            m_message_handler->OnWindowClose(msg.window);
            break;
        }
        case WM_SIZE:
        {
            const i32 width = LOWORD(msg.param_l);
            const i32 height = HIWORD(msg.param_l);
            m_message_handler->OnWindowSizeChanged(msg.window, width, height);
            break;
        }
    }
}