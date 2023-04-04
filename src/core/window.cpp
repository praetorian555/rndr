#include "rndr/core/window.h"

#include <map>

#if defined RNDR_WINDOWS
#include <windows.h>
#include <windowsx.h>
#include <winuser.h>
#endif  // RNDR_WINDOWS

#include "rndr/core/input.h"
#include "rndr/core/stack-array.h"

namespace
{
// Function declarations
LRESULT CALLBACK WindowProc(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l);

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

std::map<uint32_t, rndr::InputPrimitive> g_primitive_mapping = {
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

struct WindowData
{
    rndr::WindowDesc desc;
    HWND handle = nullptr;
    bool is_closed = false;
    bool is_minimized = false;
    bool is_maximized = false;
    bool is_visible = true;
    rndr::Window* window = nullptr;
};
}  // namespace

rndr::Window* rndr::Window::Create(const WindowDesc& desc)
{
    if (desc.width == 0 || desc.height == 0)
    {
        RNDR_LOG_ERROR("Window width and height must be greater than 0.");
        return nullptr;
    }
    if (desc.start_minimized && desc.start_maximized)
    {
        RNDR_LOG_ERROR("Window cannot be both minimized and maximized at the same time.");
        return nullptr;
    }

    // TODO(Marko): This will get the handle to the exe, should pass in the name of this dll if we
    // use dynamic linking
    HMODULE instance = GetModuleHandle(nullptr);
    const char* class_name = "RndrWindowClass";

    // TODO(Marko): Expand window functionality to provide more control to the user when creating
    // one
    WNDCLASS window_class{};
    if (!GetClassInfo(instance, class_name, &window_class))
    {
        window_class.lpszClassName = class_name;
        window_class.hInstance = instance;
        window_class.lpfnWndProc = WindowProc;
        window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;

        const ATOM atom = RegisterClass(&window_class);
        if (atom == 0)
        {
            RNDR_LOG_ERROR("Failed to register window class!");
            return {};
        }
    }

    RECT window_rect = {0, 0, desc.width, desc.height};
    DWORD window_style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    if (desc.resizable)
    {
        window_style |= WS_THICKFRAME;
    }
    if (desc.start_minimized)
    {
        window_style |= WS_MINIMIZE;
    }
    if (desc.start_maximized)
    {
        window_style |= WS_MAXIMIZE;
    }
    if (desc.start_visible)
    {
        window_style |= WS_VISIBLE;
    }

    AdjustWindowRect(&window_rect, window_style, FALSE);
    WindowData* window_data = RNDR_DEFAULT_NEW(WindowData, desc.name);
    assert(window_data != nullptr && "Failed to allocate window data!");
    HWND window_handle = CreateWindowEx(0,
                                        class_name,
                                        desc.name,
                                        window_style,
                                        CW_USEDEFAULT,
                                        CW_USEDEFAULT,
                                        window_rect.right - window_rect.left,
                                        window_rect.bottom - window_rect.top,
                                        nullptr,
                                        nullptr,
                                        instance,
                                        window_data);
    if (window_handle == nullptr)
    {
        Free(window_data);
        RNDR_LOG_ERROR("CreateWindowEx failed!");
        return {};
    }

    // Setup raw input
    constexpr uint16_t k_hid_usage_page_generic = 0x01;
    constexpr uint16_t k_hid_usage_generic_mouse = 0x02;
    StackArray<RAWINPUTDEVICE, 1> raw_devices;
    raw_devices[0].usUsagePage = k_hid_usage_page_generic;
    raw_devices[0].usUsage = k_hid_usage_generic_mouse;
    raw_devices[0].dwFlags = RIDEV_INPUTSINK;
    raw_devices[0].hwndTarget = window_handle;
    RegisterRawInputDevices(raw_devices.data(), 1, sizeof(raw_devices[0]));

    Window* window = RNDR_DEFAULT_NEW(Window, desc.name);
    window->m_window_data = reinterpret_cast<OpaquePtr>(window_data);
    window_data->window = window;
    window_data->desc = desc;
    window_data->handle = window_handle;
    window_data->is_visible = desc.start_visible;
    window_data->is_minimized = desc.start_minimized;
    window_data->is_maximized = desc.start_maximized;

    window->SetCursorMode(desc.cursor_mode);

    return window;
}

bool rndr::Window::Destroy(rndr::Window& window)
{
    WindowData* window_data = reinterpret_cast<WindowData*>(window.m_window_data);
    if (window_data->handle != nullptr)
    {
        const BOOL status = DestroyWindow(window_data->handle);
        if (status == 0)
        {
            RNDR_LOG_ERROR("Failed to destroy window!");
            return false;
        }
    }
    RNDR_DELETE(WindowData, window_data);
    RNDR_DELETE(Window, &window);
    return true;
}

bool rndr::Window::IsValid(const rndr::Window& window)
{
    return window.m_window_data != nullptr;
}

void rndr::Window::ProcessEvents() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    MSG msg{};

    while (PeekMessage(&msg, window_data->handle, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void rndr::Window::Close() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    PostMessage(window_data->handle, WM_CLOSE, 0, 0);
}

bool rndr::Window::IsClosed() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    return window_data->is_closed;
}

rndr::OpaquePtr rndr::Window::GetNativeWindowHandle() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    return reinterpret_cast<OpaquePtr>(window_data->handle);
}

int rndr::Window::GetWidth() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    return window_data->desc.width;
}

int rndr::Window::GetHeight() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    return window_data->desc.height;
}

math::Vector2 rndr::Window::GetSize() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    math::Vector2 size;
    size.X = static_cast<real>(window_data->desc.width);
    size.Y = static_cast<real>(window_data->desc.height);
    return size;
}

bool rndr::Window::Resize(int width, int height) const
{
    if (width <= 0 || height <= 0)
    {
        RNDR_LOG_ERROR("Invalid window size!");
        return false;
    }

    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    // Define the desired client area size
    RECT rc = { 0, 0, width, height };

    // Calculate the required window size, including the frame and borders
    DWORD const dw_style = GetWindowLong(window_data->handle, GWL_STYLE);
    DWORD const dw_ex_style = GetWindowLong(window_data->handle, GWL_EXSTYLE);
    BOOL const has_menu = static_cast<const BOOL>(GetMenu(window_data->handle) != nullptr);
    BOOL status = AdjustWindowRectEx(&rc, dw_style, has_menu, dw_ex_style);
    if (status == 0)
    {
        RNDR_LOG_ERROR("Failed to adjust window rect!");
        return false;
    }

    // Calculate the new window width and height
    int const new_width = rc.right - rc.left;
    int const new_height = rc.bottom - rc.top;

    // Set the window size without changing its position
    const UINT flags = SWP_NOZORDER | SWP_NOMOVE;
    status = SetWindowPos(window_data->handle, nullptr, 0, 0, new_width, new_height, flags);
    if (status == 0)
    {
        RNDR_LOG_ERROR("Failed to set window position!");
        return false;
    }
    return true;
}

bool rndr::Window::IsWindowMinimized() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    return window_data->is_minimized;
}

void rndr::Window::SetMinimized(bool should_minimize) const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    PostMessage(window_data->handle, WM_SYSCOMMAND, should_minimize ? SC_MINIMIZE : SC_RESTORE, 0);
}

void rndr::Window::SetMaximized(bool should_maximize) const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    PostMessage(window_data->handle, WM_SYSCOMMAND, should_maximize ? SC_MAXIMIZE : SC_RESTORE, 0);
}

bool rndr::Window::IsWindowMaximized() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    return window_data->is_maximized;
}

void rndr::Window::SetVisible(bool visible) const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    ShowWindow(window_data->handle, visible ? SW_SHOW : SW_HIDE);
    window_data->is_visible = visible;
}

bool rndr::Window::IsVisible() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    return window_data->is_visible && !window_data->is_minimized;
}

namespace
{
bool IsCursorHidden();
POINT GetWindowMidPointInScreenSpace(WindowData* window_data);
}  // namespace

bool rndr::Window::SetCursorMode(CursorMode mode) const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    window_data->desc.cursor_mode = mode;

    switch (mode)
    {
        case CursorMode::Normal:
        {
            ClipCursor(nullptr);
            if (IsCursorHidden())
            {
                ShowCursor(TRUE);
            }
            break;
        }
        case CursorMode::Hidden:
        {
            ClipCursor(nullptr);
            if (!IsCursorHidden())
            {
                ShowCursor(FALSE);
            }
            break;
        }
        case CursorMode::Infinite:
        {
            RECT window_rect;
            GetWindowRect(window_data->handle, &window_rect);
            ClipCursor(&window_rect);
            if (!IsCursorHidden())
            {
                ShowCursor(FALSE);
            }
            const POINT mid_point = GetWindowMidPointInScreenSpace(window_data);
            SetCursorPos(mid_point.x, mid_point.y);
            break;
        }
    }

    return true;
}

rndr::CursorMode rndr::Window::GetCursorMode() const
{
    WindowData* window_data = reinterpret_cast<WindowData*>(m_window_data);
    return window_data->desc.cursor_mode;
}

namespace
{
bool IsCursorHidden()
{
    CURSORINFO cursor_info;
    cursor_info.cbSize = sizeof(CURSORINFO);
    GetCursorInfo(&cursor_info);
    return cursor_info.flags != CURSOR_SHOWING;
}

POINT GetWindowMidPointInScreenSpace(WindowData* window_data)
{
    RECT window_rect;
    GetWindowRect(window_data->handle, &window_rect);
    const int width = window_rect.right - window_rect.left;
    const int height = window_rect.bottom - window_rect.top;
    const int mid_x = window_rect.left + width / 2;
    const int mid_y = window_rect.top + height / 2;
    return {mid_x, mid_y};
}

void HandleMouseMove(WindowData* window_data, int x, int y)
{
    if (window_data == nullptr)
    {
        return;
    }

    // We need to flip Y since the engine expects Y to grow from bottom to top
    y = window_data->desc.height - y;

    const rndr::CursorMode mode = window_data->desc.cursor_mode;
    switch (mode)
    {
        case rndr::CursorMode::Normal:
        case rndr::CursorMode::Hidden:
        {
            // Notify the input system
            rndr::InputSystem* input_system = rndr::InputSystem::Get();
            if (input_system != nullptr)
            {
                const math::Point2 absolute_position(static_cast<float>(x), static_cast<float>(y));
                input_system->SubmitMousePositionEvent(window_data->handle,
                                                       absolute_position,
                                                       window_data->window->GetSize());
            }
            break;
        }
        case rndr::CursorMode::Infinite:
        {
            const POINT mid_point = GetWindowMidPointInScreenSpace(window_data);
            SetCursorPos(mid_point.x, mid_point.y);
            break;
        }
    }
}

rndr::InputPrimitive GetPrimitive(UINT msg_code)
{
    switch (msg_code)
    {
        case WM_LBUTTONDOWN:
            [[fallthrough]];
        case WM_LBUTTONUP:
            [[fallthrough]];
        case WM_LBUTTONDBLCLK:
            return rndr::InputPrimitive::Mouse_LeftButton;
        case WM_RBUTTONDOWN:
            [[fallthrough]];
        case WM_RBUTTONUP:
            [[fallthrough]];
        case WM_RBUTTONDBLCLK:
            return rndr::InputPrimitive::Mouse_RightButton;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
            return rndr::InputPrimitive::Mouse_MiddleButton;
        default:
            assert(false);
    }
    return rndr::InputPrimitive::Count;
}

rndr::InputTrigger GetTrigger(UINT msg_code)
{
    switch (msg_code)
    {
        case WM_LBUTTONDOWN:
            [[fallthrough]];
        case WM_RBUTTONDOWN:
            [[fallthrough]];
        case WM_MBUTTONDOWN:
            return rndr::InputTrigger::ButtonDown;
        case WM_LBUTTONUP:
            [[fallthrough]];
        case WM_RBUTTONUP:
            [[fallthrough]];
        case WM_MBUTTONUP:
            return rndr::InputTrigger::ButtonUp;
        case WM_LBUTTONDBLCLK:
            [[fallthrough]];
        case WM_RBUTTONDBLCLK:
            [[fallthrough]];
        case WM_MBUTTONDBLCLK:
            return rndr::InputTrigger::DoubleClick;
        default:
            assert(false);
    }
    return rndr::InputTrigger::ButtonDown;
}

LRESULT CALLBACK WindowProc(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l)
{
    const LONG_PTR window_data_ptr = GetWindowLongPtr(window_handle, GWLP_USERDATA);
    // Before window is created this will be nullptr
    WindowData* window_data = reinterpret_cast<WindowData*>(window_data_ptr);  // NOLINT

    //    if (Window != nullptr)
    //    {
    //        rndr::Window::NativeWindowEventDelegate Delegate =
    //        Window->GetNativeWindowEventDelegate(); if
    //        (Delegate.Execute(Window->GetNativeWindowHandle(), msg_code, param_w, param_l))
    //        {
    //            return TRUE;
    //        }
    //    }

    switch (msg_code)
    {
        case WM_CREATE:
        {
            RNDR_LOG_INFO("WindowProc: Event WM_CREATE");
            CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(param_l);  // NOLINT
            window_data = reinterpret_cast<WindowData*>(CreateStruct->lpCreateParams);
            SetWindowLongPtr(window_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window_data));
            break;
        }
        case WM_SIZE:
        {
            if (window_data == nullptr || window_data->window == nullptr)
            {
                break;
            }
            const int32_t width = static_cast<int32_t>(LOWORD(param_l));
            const int32_t height = static_cast<int32_t>(HIWORD(param_l));
            RNDR_LOG_INFO("WindowProc: Event WM_SIZE (%d, %d)", width, height);
            window_data->desc.width = width;
            window_data->desc.height = height;
            window_data->window->on_resize.Execute(width, height);
            break;
        }
        case WM_CLOSE:
        {
            RNDR_LOG_INFO("WindowProc: Event WM_CLOSE");
            window_data->is_closed = true;
            break;
        }
        case WM_DESTROY:
        {
            RNDR_LOG_INFO("WindowProc: Event WM_DESTROY");
            window_data->handle = nullptr;
            break;
        }
        case WM_QUIT:
        {
            RNDR_LOG_INFO("WindowProc: Event WM_QUIT");
            break;
        }
        case WM_SYSCOMMAND:
        {
            uint16_t mask = param_w & 0xFFF0;
            if (mask == SC_MINIMIZE)
            {
                RNDR_LOG_INFO("WindowProc: Event SC_MINIMIZE");
                window_data->is_minimized = true;
                window_data->is_maximized = false;
                break;
            }
            if (mask == SC_MAXIMIZE)
            {
                RNDR_LOG_INFO("WindowProc: Event SC_MAXIMIZE");
                window_data->is_minimized = false;
                window_data->is_maximized = true;
                break;
            }
            if (mask == SC_RESTORE)
            {
                RNDR_LOG_INFO("WindowProc: Event SC_RESTORE");
                window_data->is_minimized = false;
                window_data->is_maximized = false;
                break;
            }
            break;
        }
        case WM_MOUSEMOVE:
        {
            const int x = GET_X_LPARAM(param_l);
            const int y = GET_Y_LPARAM(param_l);
            HandleMouseMove(window_data, x, y);
            break;
        }
        case WM_INPUT:
        {
            // Handling infinite mouse cursor mode
            if (window_data->desc.cursor_mode != rndr::CursorMode::Infinite)
            {
                break;
            }

            UINT struct_size = sizeof(RAWINPUT);
            rndr::StackArray<uint8_t, sizeof(RAWINPUT)> data_buffer;

            // NOLINTNEXTLINE
            GetRawInputData(reinterpret_cast<HRAWINPUT>(param_l),
                            RID_INPUT,
                            data_buffer.data(),
                            &struct_size,
                            sizeof(RAWINPUTHEADER));

            RAWINPUT* raw_data = reinterpret_cast<RAWINPUT*>(data_buffer.data());

            if (raw_data->header.dwType == RIM_TYPEMOUSE)
            {
                rndr::InputSystem* input_system = rndr::InputSystem::Get();
                if (input_system != nullptr)
                {
                    const math::Vector2 size = window_data->window->GetSize();
                    const math::Vector2 delta{
                        static_cast<float>(raw_data->data.mouse.lLastX) / size.X,
                        static_cast<float>(raw_data->data.mouse.lLastY) / size.Y};
                    input_system->SubmitRelativeMousePositionEvent(window_data->handle,
                                                                   delta,
                                                                   size);
                }
            }
            break;
        }
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        {
            if (window_data == nullptr)
            {
                break;
            }

            rndr::InputPrimitive primitive = GetPrimitive(msg_code);
            rndr::InputTrigger trigger = GetTrigger(msg_code);
            rndr::InputSystem* is = rndr::InputSystem::Get();
            if (is != nullptr)
            {
                is->SubmitButtonEvent(window_data->handle, primitive, trigger);
            }

            break;
        }
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            assert(window_data);
            const auto iter = g_primitive_mapping.find(static_cast<uint32_t>(param_w));
            if (iter == g_primitive_mapping.end())
            {
                break;
            }
            const rndr::InputPrimitive primitive = iter->second;
            const rndr::InputTrigger trigger = msg_code == WM_KEYDOWN
                                                   ? rndr::InputTrigger::ButtonDown
                                                   : rndr::InputTrigger::ButtonUp;

            rndr::InputSystem* is = rndr::InputSystem::Get();
            if (is != nullptr)
            {
                is->SubmitButtonEvent(window_data->handle, primitive, trigger);
            }

            break;
        }
        case WM_MOUSEWHEEL:
        {
            assert(window_data);
            const int delta_wheel = GET_WHEEL_DELTA_WPARAM(param_w);

            rndr::InputSystem* is = rndr::InputSystem::Get();
            if (is != nullptr)
            {
                is->SubmitMouseWheelEvent(window_data->handle, delta_wheel);
            }

            break;
        }
    }

    return DefWindowProc(window_handle, msg_code, param_w, param_l);
}
}  // namespace
