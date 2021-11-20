#include "rndr/window.h"

#include <cassert>

#include <Windows.h>

#include "log/log.h"

// Defining window deleages

rndr::WindowDelegates::ResizeDelegate rndr::WindowDelegates::OnResize;

// Window

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

rndr::Window::Window(const rndr::WindowOptions& Options) : m_Options(Options)
{
    rndr::WindowDelegates::OnResize.Add(
        [this](Window* Wind, int Width, int Height)
        {
            if (this == Wind)
            {
                Resize(Width, Height);
            }
        });

    // TODO(mkostic): This will get the handle to the exe, should pass in the name of this dll if we
    // use dynamic linking
    HMODULE Instance = GetModuleHandle(nullptr);
    const char* ClassName = "RndrWindowClass";

    WNDCLASS WindowClass{};
    WindowClass.lpszClassName = ClassName;
    WindowClass.hInstance = Instance;
    WindowClass.lpfnWndProc = WindowProc;
    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;

    ATOM Atom = RegisterClass(&WindowClass);
    assert(Atom != 0);

    HWND WindowHandle = CreateWindowEx(0, ClassName, Options.Name.c_str(), WS_OVERLAPPEDWINDOW,
                                       CW_USEDEFAULT, CW_USEDEFAULT, Options.Width, Options.Height,
                                       nullptr, nullptr, Instance, this);
    assert(WindowHandle != NULL);

    ShowWindow(WindowHandle, SW_SHOW);

    m_NativeWindowHandle = reinterpret_cast<uintptr_t>(WindowHandle);
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
    return m_NativeWindowHandle == 0;
}

void rndr::Window::Close()
{
    HWND WindowHandle = reinterpret_cast<HWND>(m_NativeWindowHandle);
    bool Result = DestroyWindow(WindowHandle);
    assert(Result);
    m_NativeWindowHandle = 0;
}

void rndr::Window::Resize(int Width, int Height)
{
    m_CurrentWidth = Width;
    m_CurrentHeight = Height;
    m_Surface.UpdateSize(Width, Height);
}

void rndr::Window::RenderToWindow()
{
    HWND WindowHandle = reinterpret_cast<HWND>(m_NativeWindowHandle);

    BITMAPINFO BitmapInfo = {};
    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = m_Surface.GetWidth();
    BitmapInfo.bmiHeader.biHeight = m_Surface.GetHeight();
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = m_Surface.GetPixelSize() * 4;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    HDC DC = GetDC(WindowHandle);

    StretchDIBits(DC, 0, 0, m_CurrentWidth, m_CurrentHeight, 0, 0, m_Surface.GetWidth(), m_Surface.GetHeight(),
                  m_Surface.GetColorBuffer(), &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK WindowProc(HWND WindowHandle, UINT MsgCode, WPARAM ParamW, LPARAM ParamL)
{
    LRESULT Result = 0;

    switch (MsgCode)
    {
        case WM_CREATE:
        {
            RNDR_INFO("WindowProc: Event WM_CREATE");

            CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(ParamL);
            rndr::Window* Wind = reinterpret_cast<rndr::Window*>(CreateStruct->lpCreateParams);

            SetWindowLongPtr(WindowHandle, GWLP_USERDATA, (LONG_PTR)Wind);

            break;
        }
        case WM_SIZE:
        {
            UINT Width = LOWORD(ParamL);
            UINT Height = HIWORD(ParamL);

            RNDR_INFO("WindowProc: Event WM_SIZE (%d, %d)", Width, Height);

            LONG_PTR Ptr = GetWindowLongPtr(WindowHandle, GWLP_USERDATA);
            rndr::Window* Wind = reinterpret_cast<rndr::Window*>(Ptr);

            rndr::WindowDelegates::OnResize.Execute(std::move(Wind), Width, Height);

            break;
        }
        case WM_CLOSE:
        {
            RNDR_INFO("WindowProc: Event WM_CLOSE");

            rndr::Window* Wind =
                reinterpret_cast<rndr::Window*>(GetWindowLongPtr(WindowHandle, GWLP_USERDATA));
            assert(Wind);

            Wind->Close();

            break;
        }
        case WM_DESTROY:
        {
            RNDR_INFO("WindowProc: Event WM_DESTROY");

            break;
        }
        case WM_QUIT:
        {
            RNDR_INFO("WindowProc: Event WM_QUIT");

            break;
        }
    }

    return DefWindowProc(WindowHandle, MsgCode, ParamW, ParamL);
}
