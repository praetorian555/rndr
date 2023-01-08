#include "rndr/core/window.h"

#include <cassert>
#include <map>

#if defined RNDR_WINDOWS
#include <Windows.h>
#include <windowsx.h>
#endif  // RNDR_WINDOWS

#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/rndrcontext.h"

// Missing virtual keys in the Windows API
enum WindowsVirtualKey : uint32_t
{
    VK_A = 0x41,
    VK_W = 0x57,
    VK_S = 0x53,
    VK_E = 0x45,
    VK_Q = 0x51,
    VK_D = 0x44,
    VK_I = 0x49,
    VK_J = 0x4A,
    VK_K = 0x4B,
    VK_L = 0x4C,
};

static std::map<uint32_t, rndr::InputPrimitive> GPrimitiveMapping = {
    {VK_A, rndr::InputPrimitive::Keyboard_A},
    {VK_W, rndr::InputPrimitive::Keyboard_W},
    {VK_S, rndr::InputPrimitive::Keyboard_S},
    {VK_E, rndr::InputPrimitive::Keyboard_E},
    {VK_Q, rndr::InputPrimitive::Keyboard_Q},
    {VK_D, rndr::InputPrimitive::Keyboard_D},
    {VK_I, rndr::InputPrimitive::Keyboard_I},
    {VK_J, rndr::InputPrimitive::Keyboard_J},
    {VK_K, rndr::InputPrimitive::Keyboard_K},
    {VK_L, rndr::InputPrimitive::Keyboard_L},
    {VK_ESCAPE, rndr::InputPrimitive::Keyboard_Esc},
    {VK_LEFT, rndr::InputPrimitive::Keyboard_Left},
    {VK_UP, rndr::InputPrimitive::Keyboard_Up},
    {VK_RIGHT, rndr::InputPrimitive::Keyboard_Right},
    {VK_DOWN, rndr::InputPrimitive::Keyboard_Down}};

// Defining window deleages

rndr::WindowDelegates::ResizeDelegate rndr::WindowDelegates::OnResize;
rndr::WindowDelegates::ButtonDelegate rndr::WindowDelegates::OnButtonDelegate;
rndr::WindowDelegates::MousePositionDelegate rndr::WindowDelegates::OnMousePositionDelegate;
rndr::WindowDelegates::MouseWheelDelegate rndr::WindowDelegates::OnMouseWheelMovedDelegate;

// Window

LRESULT CALLBACK WindowProc(HWND WindowHandle, UINT MsgCode, WPARAM ParamW, LPARAM ParamL);

rndr::Window::~Window()
{
    if (!IsClosed())
    {
        Close();
    }
}

bool rndr::Window::Init(int Width, int Height, const WindowProperties& Props)
{
    m_Width = Width;
    m_Height = Height;
    m_Props = Props;

    rndr::WindowDelegates::OnResize.Add(RNDR_BIND_THREE_PARAM(this, &Window::Resize));
    rndr::WindowDelegates::OnButtonDelegate.Add(RNDR_BIND_THREE_PARAM(this, &Window::ButtonEvent));

    // TODO(Marko): This will get the handle to the exe, should pass in the name of this dll if we
    // use dynamic linking
    HMODULE Instance = GetModuleHandle(nullptr);
    const char* ClassName = "RndrWindowClass";

    // TODO(Marko): Expand window functionality to provide more control to the user when creating
    // one
    WNDCLASS WindowClass{};
    if (!GetClassInfo(Instance, ClassName, &WindowClass))
    {
        WindowClass.lpszClassName = ClassName;
        WindowClass.hInstance = Instance;
        WindowClass.lpfnWndProc = WindowProc;
        WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;

        const ATOM Atom = RegisterClass(&WindowClass);
        if (Atom == 0)
        {
            RNDR_LOG_ERROR("Window::Init: Failed to register window class!");
            return false;
        }
    }

    RECT WindowRect = {0, 0, m_Width, m_Height};
    AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND WindowHandle =
        CreateWindowEx(0, ClassName, m_Props.Name.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                       CW_USEDEFAULT, WindowRect.right - WindowRect.left,
                       WindowRect.bottom - WindowRect.top, nullptr, nullptr, Instance, this);
    if (WindowHandle == nullptr)
    {
        RNDR_LOG_ERROR("Window::Init: CreateWindowEx failed!");
        return false;
    }

    ShowWindow(WindowHandle, SW_SHOW);

    m_NativeWindowHandle = reinterpret_cast<uintptr_t>(WindowHandle);

    return true;
}

void rndr::Window::ProcessEvents() const
{
    HWND WindowHandle = reinterpret_cast<HWND>(m_NativeWindowHandle);
    MSG Msg{};

    while (PeekMessage(&Msg, WindowHandle, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
}

bool rndr::Window::IsClosed() const
{
    return m_NativeWindowHandle == 0;
}

void rndr::Window::Close()
{
    HWND WindowHandle = reinterpret_cast<HWND>(m_NativeWindowHandle);
    const bool Result = DestroyWindow(WindowHandle) > 0;
    if (!Result)
    {
        RNDR_LOG_WARNING("Window::Close: Failed to destroy the native window!");
    }
    m_NativeWindowHandle = 0;
}

rndr::NativeWindowHandle rndr::Window::GetNativeWindowHandle() const
{
    return m_NativeWindowHandle;
}

int rndr::Window::GetWidth() const
{
    return m_Width;
}

int rndr::Window::GetHeight() const
{
    return m_Height;
}

bool rndr::Window::IsWindowMinimized() const
{
    return m_Width == 0 || m_Height == 0;
}

void rndr::Window::LockCursor(bool ShouldLock) const
{
    if (ShouldLock)
    {
        RECT WindowRect;
        HWND WindowHandle = reinterpret_cast<HWND>(m_NativeWindowHandle);
        GetWindowRect(WindowHandle, &WindowRect);
        ClipCursor(&WindowRect);
    }
    else
    {
        ClipCursor(nullptr);
    }
}

void rndr::Window::ActivateInfiniteCursor(bool Activate)
{
    if (m_InifiniteCursor == Activate)
    {
        return;
    }

    m_InifiniteCursor = Activate;
    ShowCursor(static_cast<int>(!m_InifiniteCursor));
}

void rndr::Window::ButtonEvent(Window* Window, InputPrimitive Primitive, InputTrigger Trigger)
{
    RNDR_UNUSED(Window);
    if (Primitive == InputPrimitive::Keyboard_Esc && Trigger == InputTrigger::ButtonDown)
    {
        Close();
    }
}

void rndr::Window::Resize(Window* Window, int Width, int Height)
{
    if (Window != this)
    {
        return;
    }

    m_Width = Width;
    m_Height = Height;
}

LRESULT CALLBACK WindowProc(HWND WindowHandle, UINT MsgCode, WPARAM ParamW, LPARAM ParamL)
{
    const LONG_PTR WindowPtr = GetWindowLongPtr(WindowHandle, GWLP_USERDATA);
    rndr::Window* Window = reinterpret_cast<rndr::Window*>(WindowPtr);

    switch (MsgCode)
    {
        case WM_CREATE:
        {
            RNDR_LOG_INFO("WindowProc: Event WM_CREATE");

            CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(ParamL);
            Window = reinterpret_cast<rndr::Window*>(CreateStruct->lpCreateParams);

            SetWindowLongPtr(WindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Window));

            break;
        }
        case WM_SIZE:
        {
            const uint32_t Width = LOWORD(ParamL);
            const uint32_t Height = HIWORD(ParamL);

            RNDR_LOG_INFO("WindowProc: Event WM_SIZE (%d, %d)", Width, Height);

            rndr::WindowDelegates::OnResize.Execute(Window, Width, Height);

            break;
        }
        case WM_CLOSE:
        {
            RNDR_LOG_INFO("WindowProc: Event WM_CLOSE");

            Window->Close();

            break;
        }
        case WM_DESTROY:
        {
            RNDR_LOG_INFO("WindowProc: Event WM_DESTROY");

            break;
        }
        case WM_QUIT:
        {
            RNDR_LOG_INFO("WindowProc: Event WM_QUIT");

            break;
        }
        case WM_MOUSEMOVE:
        {
            static bool s_CursorPositionChanged = false;

            const int X = GET_X_LPARAM(ParamL);
            // In RNDR y grows from bottom to up
            const int Y = Window->GetHeight() - GET_Y_LPARAM(ParamL);

            if (Window->IsInfiniteCursor() && s_CursorPositionChanged)
            {
                s_CursorPositionChanged = false;
                break;
            }

            rndr::WindowDelegates::OnMousePositionDelegate.Execute(Window, X, Y);

            // Notify the input system
            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                const math::Point2 AbsolutePosition(static_cast<float>(X), static_cast<float>(Y));
                const math::Vector2 ScreenSize(static_cast<float>(Window->GetWidth()),
                                               static_cast<float>(Window->GetHeight()));
                IS->SubmitMousePositionEvent(Window->GetNativeWindowHandle(), AbsolutePosition,
                                             ScreenSize);
            }

            if (Window->IsInfiniteCursor())
            {
                RECT WindowRect;
                GetWindowRect(WindowHandle, &WindowRect);
                POINT NewCursorPosition;
                NewCursorPosition.x = WindowRect.left + Window->GetWidth() / 2;
                NewCursorPosition.y = WindowRect.top + Window->GetHeight() / 2;
                SetCursorPos(NewCursorPosition.x, NewCursorPosition.y);
                s_CursorPositionChanged = true;
            }

            break;
        }
        case WM_LBUTTONDBLCLK:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(
                Window, rndr::InputPrimitive::Mouse_LeftButton, rndr::InputTrigger::DoubleClick);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitButtonEvent(Window->GetNativeWindowHandle(),
                                      rndr::InputPrimitive::Mouse_LeftButton,
                                      rndr::InputTrigger::DoubleClick);
            }

            break;
        }
        case WM_RBUTTONDBLCLK:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(
                Window, rndr::InputPrimitive::Mouse_RightButton, rndr::InputTrigger::DoubleClick);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitButtonEvent(Window->GetNativeWindowHandle(),
                                      rndr::InputPrimitive::Mouse_RightButton,
                                      rndr::InputTrigger::DoubleClick);
            }

            break;
        }
        case WM_MBUTTONDBLCLK:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(
                Window, rndr::InputPrimitive::Mouse_MiddleButton, rndr::InputTrigger::DoubleClick);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitButtonEvent(Window->GetNativeWindowHandle(),
                                      rndr::InputPrimitive::Mouse_MiddleButton,
                                      rndr::InputTrigger::DoubleClick);
            }

            break;
        }
        case WM_LBUTTONDOWN:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(
                Window, rndr::InputPrimitive::Mouse_LeftButton, rndr::InputTrigger::ButtonDown);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitButtonEvent(Window->GetNativeWindowHandle(),
                                      rndr::InputPrimitive::Mouse_LeftButton,
                                      rndr::InputTrigger::ButtonDown);
            }

            break;
        }
        case WM_LBUTTONUP:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(
                Window, rndr::InputPrimitive::Mouse_LeftButton, rndr::InputTrigger::ButtonUp);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitButtonEvent(Window->GetNativeWindowHandle(),
                                      rndr::InputPrimitive::Mouse_LeftButton,
                                      rndr::InputTrigger::ButtonUp);
            }

            break;
        }
        case WM_RBUTTONDOWN:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(
                Window, rndr::InputPrimitive::Mouse_RightButton, rndr::InputTrigger::ButtonDown);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitButtonEvent(Window->GetNativeWindowHandle(),
                                      rndr::InputPrimitive::Mouse_RightButton,
                                      rndr::InputTrigger::ButtonDown);
            }

            break;
        }
        case WM_RBUTTONUP:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(
                Window, rndr::InputPrimitive::Mouse_RightButton, rndr::InputTrigger::ButtonUp);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitButtonEvent(Window->GetNativeWindowHandle(),
                                      rndr::InputPrimitive::Mouse_RightButton,
                                      rndr::InputTrigger::ButtonUp);
            }

            break;
        }
        case WM_MBUTTONDOWN:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(
                Window, rndr::InputPrimitive::Mouse_MiddleButton, rndr::InputTrigger::ButtonDown);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitButtonEvent(Window->GetNativeWindowHandle(),
                                      rndr::InputPrimitive::Mouse_MiddleButton,
                                      rndr::InputTrigger::ButtonDown);
            }

            break;
        }
        case WM_MBUTTONUP:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(
                Window, rndr::InputPrimitive::Mouse_MiddleButton, rndr::InputTrigger::ButtonUp);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitButtonEvent(Window->GetNativeWindowHandle(),
                                      rndr::InputPrimitive::Mouse_MiddleButton,
                                      rndr::InputTrigger::ButtonUp);
            }

            break;
        }
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            const auto Iter = GPrimitiveMapping.find(static_cast<uint32_t>(ParamW));
            if (Iter == GPrimitiveMapping.end())
            {
                break;
            }
            const rndr::InputPrimitive Primitive = Iter->second;
            const rndr::InputTrigger Trigger = MsgCode == WM_KEYDOWN
                                                   ? rndr::InputTrigger::ButtonDown
                                                   : rndr::InputTrigger::ButtonUp;
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, Primitive, Trigger);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitButtonEvent(Window->GetNativeWindowHandle(), Primitive, Trigger);
            }

            break;
        }
        case WM_MOUSEWHEEL:
        {
            const int DeltaWheel = GET_WHEEL_DELTA_WPARAM(ParamW);
            rndr::WindowDelegates::OnMouseWheelMovedDelegate.Execute(Window, DeltaWheel);

            rndr::InputSystem* IS = rndr::GRndrContext->GetInputSystem();
            if (IS != nullptr)
            {
                IS->SubmitMouseWheelEvent(Window->GetNativeWindowHandle(), DeltaWheel);
            }

            break;
        }
    }

    return DefWindowProc(WindowHandle, MsgCode, ParamW, ParamL);
}
