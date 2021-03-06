#include "rndr/core/window.h"

#include <cassert>
#include <map>

#if defined RNDR_WINDOWS
#include <Windows.h>
#include <windowsx.h>
#endif  // RNDR_WINDOWS

#include "rndr/core/input.h"
#include "rndr/core/log.h"

static std::map<uint32_t, rndr::InputPrimitive> g_PrimitiveMapping = {
    {0x41, rndr::InputPrimitive::Keyboard_A},  {0x57, rndr::InputPrimitive::Keyboard_W},     {0x53, rndr::InputPrimitive::Keyboard_S},
    {0x45, rndr::InputPrimitive::Keyboard_E},  {0x51, rndr::InputPrimitive::Keyboard_Q},     {0x44, rndr::InputPrimitive::Keyboard_D},
    {0x49, rndr::InputPrimitive::Keyboard_I},  {0x4A, rndr::InputPrimitive::Keyboard_J},     {0x4B, rndr::InputPrimitive::Keyboard_K},
    {0x4C, rndr::InputPrimitive::Keyboard_L},  {0x1B, rndr::InputPrimitive::Keyboard_Esc},   {0x25, rndr::InputPrimitive::Keyboard_Left},
    {0x26, rndr::InputPrimitive::Keyboard_Up}, {0x27, rndr::InputPrimitive::Keyboard_Right}, {0x28, rndr::InputPrimitive::Keyboard_Down}};

// Defining window deleages

rndr::WindowDelegates::ResizeDelegate rndr::WindowDelegates::OnResize;
rndr::WindowDelegates::ButtonDelegate rndr::WindowDelegates::OnButtonDelegate;
rndr::WindowDelegates::MousePositionDelegate rndr::WindowDelegates::OnMousePositionDelegate;
rndr::WindowDelegates::MouseWheelDelegate rndr::WindowDelegates::OnMouseWheelMovedDelegate;

// Window

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

rndr::Window::Window(int Width, int Height, const WindowProperties& Props) : m_Width(Width), m_Height(Height), m_Props(Props)
{
    rndr::WindowDelegates::OnResize.Add(RNDR_BIND_THREE_PARAM(this, &Window::Resize));
    rndr::WindowDelegates::OnButtonDelegate.Add(RNDR_BIND_THREE_PARAM(this, &Window::ButtonEvent));

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

    RECT WindowRect = {0, 0, m_Width, m_Height};
    AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND WindowHandle =
        CreateWindowEx(0, ClassName, m_Props.Name.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                       WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top, nullptr, nullptr, Instance, this);
    assert(WindowHandle != NULL);

    ShowWindow(WindowHandle, SW_SHOW);

    m_NativeWindowHandle = reinterpret_cast<uintptr_t>(WindowHandle);

    GraphicsContextProperties GCProps;
    GCProps.WindowWidth = m_Width;
    GCProps.WindowHeight = m_Height;
    GCProps.FrameBuffer.bWindowFrameBuffer = true;
    GCProps.FrameBuffer.ColorBufferCount = 1;
    GCProps.FrameBuffer.ColorBufferProperties[0].PixelFormat = PixelFormat::R8G8B8A8_UNORM_SRGB;
    GCProps.FrameBuffer.ColorBufferProperties[0].CPUAccess = CPUAccess::None;
    GCProps.FrameBuffer.ColorBufferProperties[0].Usage = Usage::GPUReadWrite;
    GCProps.FrameBuffer.ColorBufferProperties[0].bUseMips = false;
    GCProps.FrameBuffer.ColorBufferProperties[0].ImageBindFlags = ImageBindFlags::RenderTarget;
    GCProps.FrameBuffer.bUseDepthStencil = true;
    GCProps.FrameBuffer.DepthStencilBufferProperties.ImageBindFlags = ImageBindFlags::DepthStencil;
    GCProps.FrameBuffer.DepthStencilBufferProperties.PixelFormat = PixelFormat::DEPTH24_STENCIL8;
    GCProps.FrameBuffer.DepthStencilBufferProperties.CPUAccess = CPUAccess::None;
    GCProps.FrameBuffer.DepthStencilBufferProperties.Usage = Usage::GPUReadWrite;
    GCProps.FrameBuffer.DepthStencilBufferProperties.bUseMips = false;
    m_GraphicsContext = std::make_unique<GraphicsContext>(this, GCProps);
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

rndr::GraphicsContext* rndr::Window::GetGraphicsContext() const
{
    return m_GraphicsContext.get();
}

void rndr::Window::LockCursor(bool ShouldLock)
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
        ClipCursor(NULL);
    }
}

void rndr::Window::ActivateInfiniteCursor(bool Activate)
{
    if (m_InifiniteCursor == Activate)
    {
        return;
    }

    m_InifiniteCursor = Activate;
    ShowCursor(!m_InifiniteCursor);
}

void rndr::Window::ButtonEvent(Window* Window, InputPrimitive Primitive, InputTrigger Trigger)
{
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
    LRESULT Result = 0;

    LONG_PTR WindowPtr = GetWindowLongPtr(WindowHandle, GWLP_USERDATA);
    rndr::Window* Window = reinterpret_cast<rndr::Window*>(WindowPtr);

    switch (MsgCode)
    {
        case WM_CREATE:
        {
            RNDR_LOG_INFO("WindowProc: Event WM_CREATE");

            CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(ParamL);
            rndr::Window* Window = reinterpret_cast<rndr::Window*>(CreateStruct->lpCreateParams);

            SetWindowLongPtr(WindowHandle, GWLP_USERDATA, (LONG_PTR)Window);

            break;
        }
        case WM_SIZE:
        {
            UINT Width = LOWORD(ParamL);
            UINT Height = HIWORD(ParamL);

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
            static bool CursorPositionChanged = false;

            const int X = GET_X_LPARAM(ParamL);
            // In RNDR y grows from bottom to up
            const int Y = Window->GetHeight() - GET_Y_LPARAM(ParamL);

            if (Window->IsInfiniteCursor() && CursorPositionChanged)
            {
                CursorPositionChanged = false;
                break;
            }

            rndr::WindowDelegates::OnMousePositionDelegate.Execute(Window, X, Y);

            if (Window->IsInfiniteCursor())
            {
                RECT WindowRect;
                GetWindowRect(WindowHandle, &WindowRect);
                POINT NewCursorPosition;
                NewCursorPosition.x = WindowRect.left + Window->GetWidth() / 2;
                NewCursorPosition.y = WindowRect.top + Window->GetHeight() / 2;
                SetCursorPos(NewCursorPosition.x, NewCursorPosition.y);
                CursorPositionChanged = true;
            }

            break;
        }
        case WM_LBUTTONDBLCLK:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, rndr::InputPrimitive::Mouse_LeftButton,
                                                            rndr::InputTrigger::DoubleClick);
            break;
        }
        case WM_RBUTTONDBLCLK:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, rndr::InputPrimitive::Mouse_RightButton,
                                                            rndr::InputTrigger::DoubleClick);
            break;
        }
        case WM_MBUTTONDBLCLK:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, rndr::InputPrimitive::Mouse_MiddleButton,
                                                            rndr::InputTrigger::DoubleClick);
            break;
        }
        case WM_LBUTTONDOWN:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, rndr::InputPrimitive::Mouse_LeftButton, rndr::InputTrigger::ButtonDown);
            break;
        }
        case WM_LBUTTONUP:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, rndr::InputPrimitive::Mouse_LeftButton, rndr::InputTrigger::ButtonUp);
            break;
        }
        case WM_RBUTTONDOWN:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, rndr::InputPrimitive::Mouse_RightButton,
                                                            rndr::InputTrigger::ButtonDown);
            break;
        }
        case WM_RBUTTONUP:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, rndr::InputPrimitive::Mouse_RightButton, rndr::InputTrigger::ButtonUp);
            break;
        }
        case WM_MBUTTONDOWN:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, rndr::InputPrimitive::Mouse_MiddleButton,
                                                            rndr::InputTrigger::ButtonDown);
            break;
        }
        case WM_MBUTTONUP:
        {
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, rndr::InputPrimitive::Mouse_MiddleButton, rndr::InputTrigger::ButtonUp);
            break;
        }
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            const auto Iter = g_PrimitiveMapping.find(ParamW);
            if (Iter == g_PrimitiveMapping.end())
            {
                break;
            }
            const rndr::InputPrimitive Primitive = Iter->second;
            const rndr::InputTrigger Trigger = MsgCode == WM_KEYDOWN ? rndr::InputTrigger::ButtonDown : rndr::InputTrigger::ButtonUp;
            rndr::WindowDelegates::OnButtonDelegate.Execute(Window, Primitive, Trigger);
            break;
        }
        case WM_MOUSEWHEEL:
        {
            int DeltaWheel = GET_WHEEL_DELTA_WPARAM(ParamW);
            rndr::WindowDelegates::OnMouseWheelMovedDelegate.Execute(Window, DeltaWheel);
            break;
        }
    }

    return DefWindowProc(WindowHandle, MsgCode, ParamW, ParamL);
}
