#include "rndr/window.h"

#include <cassert>

#include <Windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

rndr::Window::Window(const rndr::WindowOptions& Options) : m_Options(Options)
{
    // TODO(mkostic): This will get the handle to the exe, should pass in the name of this dll if we
    // use dynamic linking
    HMODULE Instance = GetModuleHandle(nullptr);
    const char* ClassName = "RndrWindowClass";

    WNDCLASS WindowClass{};
    WindowClass.lpszClassName = ClassName;
    WindowClass.hInstance = Instance;
    WindowClass.lpfnWndProc = WindowProc;

    RegisterClass(&WindowClass);

    HWND WindowHandle = CreateWindowEx(0, ClassName, Options.Name.c_str(), WS_OVERLAPPEDWINDOW,
                                       CW_USEDEFAULT, CW_USEDEFAULT, Options.Width, Options.Height,
                                       nullptr, nullptr, Instance, this);
    assert(WindowHandle != NULL);

    ShowWindow(WindowHandle, SW_SHOW);

    m_NativeWindowHandle = reinterpret_cast<void*>(WindowHandle);
}

void rndr::Window::ProcessEvents()
{
    HWND WindowHandle = reinterpret_cast<HWND>(m_NativeWindowHandle);
    MSG msg{};

    while (PeekMessage(&msg, WindowHandle, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

bool rndr::Window::IsClosed() const
{
    return m_NativeWindowHandle == nullptr;
}

void rndr::Window::Close()
{
    HWND WindowHandle = reinterpret_cast<HWND>(m_NativeWindowHandle);
    bool Result = DestroyWindow(WindowHandle);
    assert(Result);
    m_NativeWindowHandle = nullptr;
}

LRESULT CALLBACK WindowProc(HWND WindowHandle, UINT MsgCode, WPARAM ParamW, LPARAM ParamL)
{
    switch (MsgCode)
    {
        case WM_CREATE:
        {
            CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(ParamL);
            rndr::Window* Wind = reinterpret_cast<rndr::Window*>(CreateStruct->lpCreateParams);

            SetWindowLongPtr(WindowHandle, GWLP_USERDATA, (LONG_PTR)Wind);

            break;
        }
        case WM_CLOSE:
        {
            rndr::Window* Wind = reinterpret_cast<rndr::Window*>(GetWindowLongPtr(WindowHandle, GWLP_USERDATA));
            assert(Wind);

            Wind->Close();

            break;
        }
    }

    return DefWindowProc(WindowHandle, MsgCode, ParamW, ParamL);
}
