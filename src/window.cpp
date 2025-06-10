#include "rndr/definitions.h"

#include "opal/container/hash-map.h"
#include "opal/container/in-place-array.h"

#include "rndr/input.h"
#include "rndr/log.h"
#include "rndr/platform/windows-header.h"
#include "rndr/trace.h"
#include "rndr/window.h"

#include <Windowsx.h>

namespace
{
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

const Opal::HashMap<Rndr::InputPrimitive, uint32_t> g_primitive_to_vk = {{Rndr::InputPrimitive::A, 0x41},
                                                                         {Rndr::InputPrimitive::B, 0x42},
                                                                         {Rndr::InputPrimitive::C, 0x43},
                                                                         {Rndr::InputPrimitive::D, 0x44},
                                                                         {Rndr::InputPrimitive::E, 0x45},
                                                                         {Rndr::InputPrimitive::F, 0x46},
                                                                         {Rndr::InputPrimitive::G, 0x47},
                                                                         {Rndr::InputPrimitive::H, 0x48},
                                                                         {Rndr::InputPrimitive::I, 0x49},
                                                                         {Rndr::InputPrimitive::J, 0x4A},
                                                                         {Rndr::InputPrimitive::K, 0x4B},
                                                                         {Rndr::InputPrimitive::L, 0x4C},
                                                                         {Rndr::InputPrimitive::M, 0x4D},
                                                                         {Rndr::InputPrimitive::N, 0x4E},
                                                                         {Rndr::InputPrimitive::O, 0x4F},
                                                                         {Rndr::InputPrimitive::P, 0x50},
                                                                         {Rndr::InputPrimitive::Q, 0x51},
                                                                         {Rndr::InputPrimitive::R, 0x52},
                                                                         {Rndr::InputPrimitive::S, 0x53},
                                                                         {Rndr::InputPrimitive::T, 0x54},
                                                                         {Rndr::InputPrimitive::U, 0x55},
                                                                         {Rndr::InputPrimitive::V, 0x56},
                                                                         {Rndr::InputPrimitive::W, 0x57},
                                                                         {Rndr::InputPrimitive::X, 0x58},
                                                                         {Rndr::InputPrimitive::Y, 0x59},
                                                                         {Rndr::InputPrimitive::Z, 0x5A},
                                                                         {Rndr::InputPrimitive::Digit_0, 0x30},
                                                                         {Rndr::InputPrimitive::Digit_1, 0x31},
                                                                         {Rndr::InputPrimitive::Digit_2, 0x32},
                                                                         {Rndr::InputPrimitive::Digit_3, 0x33},
                                                                         {Rndr::InputPrimitive::Digit_4, 0x34},
                                                                         {Rndr::InputPrimitive::Digit_5, 0x35},
                                                                         {Rndr::InputPrimitive::Digit_6, 0x36},
                                                                         {Rndr::InputPrimitive::Digit_7, 0x37},
                                                                         {Rndr::InputPrimitive::Digit_8, 0x38},
                                                                         {Rndr::InputPrimitive::Digit_9, 0x39},
                                                                         {Rndr::InputPrimitive::F1, VK_F1},
                                                                         {Rndr::InputPrimitive::F2, VK_F2},
                                                                         {Rndr::InputPrimitive::F3, VK_F3},
                                                                         {Rndr::InputPrimitive::F4, VK_F4},
                                                                         {Rndr::InputPrimitive::F5, VK_F5},
                                                                         {Rndr::InputPrimitive::F6, VK_F6},
                                                                         {Rndr::InputPrimitive::F7, VK_F7},
                                                                         {Rndr::InputPrimitive::F8, VK_F8},
                                                                         {Rndr::InputPrimitive::F9, VK_F9},
                                                                         {Rndr::InputPrimitive::F10, VK_F10},
                                                                         {Rndr::InputPrimitive::F11, VK_F11},
                                                                         {Rndr::InputPrimitive::F12, VK_F12},
                                                                         {Rndr::InputPrimitive::LeftShift, VK_LSHIFT},
                                                                         {Rndr::InputPrimitive::RightShift, VK_RSHIFT},
                                                                         {Rndr::InputPrimitive::LeftAlt, VK_LMENU},
                                                                         {Rndr::InputPrimitive::RightAlt, VK_RMENU},
                                                                         {Rndr::InputPrimitive::LeftCtrl, VK_LCONTROL},
                                                                         {Rndr::InputPrimitive::RightCtrl, VK_RCONTROL},
                                                                         {Rndr::InputPrimitive::LeftArrow, VK_LEFT},
                                                                         {Rndr::InputPrimitive::RightArrow, VK_RIGHT},
                                                                         {Rndr::InputPrimitive::UpArrow, VK_UP},
                                                                         {Rndr::InputPrimitive::DownArrow, VK_DOWN},
                                                                         {Rndr::InputPrimitive::Escape, VK_ESCAPE},
                                                                         {Rndr::InputPrimitive::Space, VK_SPACE}};

const Opal::HashMap<uint32_t, Rndr::InputPrimitive> g_vk_to_primitive = {{0x41, Rndr::InputPrimitive::A},
                                                                         {0x42, Rndr::InputPrimitive::B},
                                                                         {0x43, Rndr::InputPrimitive::C},
                                                                         {0x44, Rndr::InputPrimitive::D},
                                                                         {0x45, Rndr::InputPrimitive::E},
                                                                         {0x46, Rndr::InputPrimitive::F},
                                                                         {0x47, Rndr::InputPrimitive::G},
                                                                         {0x48, Rndr::InputPrimitive::H},
                                                                         {0x49, Rndr::InputPrimitive::I},
                                                                         {0x4A, Rndr::InputPrimitive::J},
                                                                         {0x4B, Rndr::InputPrimitive::K},
                                                                         {0x4C, Rndr::InputPrimitive::L},
                                                                         {0x4D, Rndr::InputPrimitive::M},
                                                                         {0x4E, Rndr::InputPrimitive::N},
                                                                         {0x4F, Rndr::InputPrimitive::O},
                                                                         {0x50, Rndr::InputPrimitive::P},
                                                                         {0x51, Rndr::InputPrimitive::Q},
                                                                         {0x52, Rndr::InputPrimitive::R},
                                                                         {0x53, Rndr::InputPrimitive::S},
                                                                         {0x54, Rndr::InputPrimitive::T},
                                                                         {0x55, Rndr::InputPrimitive::U},
                                                                         {0x56, Rndr::InputPrimitive::V},
                                                                         {0x57, Rndr::InputPrimitive::W},
                                                                         {0x58, Rndr::InputPrimitive::X},
                                                                         {0x59, Rndr::InputPrimitive::Y},
                                                                         {0x5A, Rndr::InputPrimitive::Z},
                                                                         {0x30, Rndr::InputPrimitive::Digit_0},
                                                                         {0x31, Rndr::InputPrimitive::Digit_1},
                                                                         {0x32, Rndr::InputPrimitive::Digit_2},
                                                                         {0x33, Rndr::InputPrimitive::Digit_3},
                                                                         {0x34, Rndr::InputPrimitive::Digit_4},
                                                                         {0x35, Rndr::InputPrimitive::Digit_5},
                                                                         {0x36, Rndr::InputPrimitive::Digit_6},
                                                                         {0x37, Rndr::InputPrimitive::Digit_7},
                                                                         {0x38, Rndr::InputPrimitive::Digit_8},
                                                                         {0x39, Rndr::InputPrimitive::Digit_9},
                                                                         {VK_F1, Rndr::InputPrimitive::F1},
                                                                         {VK_F2, Rndr::InputPrimitive::F2},
                                                                         {VK_F3, Rndr::InputPrimitive::F3},
                                                                         {VK_F4, Rndr::InputPrimitive::F4},
                                                                         {VK_F5, Rndr::InputPrimitive::F5},
                                                                         {VK_F6, Rndr::InputPrimitive::F6},
                                                                         {VK_F7, Rndr::InputPrimitive::F7},
                                                                         {VK_F8, Rndr::InputPrimitive::F8},
                                                                         {VK_F9, Rndr::InputPrimitive::F9},
                                                                         {VK_F10, Rndr::InputPrimitive::F10},
                                                                         {VK_F11, Rndr::InputPrimitive::F11},
                                                                         {VK_F12, Rndr::InputPrimitive::F12},
                                                                         {VK_LSHIFT, Rndr::InputPrimitive::LeftShift},
                                                                         {VK_RSHIFT, Rndr::InputPrimitive::RightShift},
                                                                         {VK_LMENU, Rndr::InputPrimitive::LeftAlt},
                                                                         {VK_RMENU, Rndr::InputPrimitive::RightAlt},
                                                                         {VK_LCONTROL, Rndr::InputPrimitive::LeftCtrl},
                                                                         {VK_RCONTROL, Rndr::InputPrimitive::RightCtrl},
                                                                         {VK_LEFT, Rndr::InputPrimitive::LeftArrow},
                                                                         {VK_RIGHT, Rndr::InputPrimitive::RightArrow},
                                                                         {VK_UP, Rndr::InputPrimitive::UpArrow},
                                                                         {VK_DOWN, Rndr::InputPrimitive::DownArrow},
                                                                         {VK_ESCAPE, Rndr::InputPrimitive::Escape},
                                                                         {VK_SPACE, Rndr::InputPrimitive::Space}};

}  // namespace

Rndr::Window::Window(const WindowDesc& desc)
{
    // Invalid window by default
    m_desc.height = 0;
    m_desc.width = 0;
    m_desc.name = nullptr;
    m_desc.resizable = false;
    m_desc.start_maximized = false;
    m_desc.start_minimized = false;
    m_desc.start_visible = false;

    if (desc.width == 0 || desc.height == 0)
    {
        RNDR_LOG_ERROR("Window width and height must be greater than 0.");
        return;
    }
    if (desc.start_minimized && desc.start_maximized)
    {
        RNDR_LOG_ERROR("Window cannot be both minimized and maximized at the same time.");
        return;
    }

    // TODO(Marko): This will get the handle to the exe, should pass in the name of this dll if we
    // use dynamic linking
    HMODULE instance = GetModuleHandle(nullptr);
    const char16* class_name = L"RndrWindowClass";

    // TODO(Marko): Expand window functionality to provide more control to the user when creating
    // one
    WNDCLASS window_class{};
    if (!GetClassInfo(instance, class_name, &window_class))
    {
        window_class.lpszClassName = class_name;
        window_class.hInstance = instance;
        window_class.lpfnWndProc = Rndr::WindowPrivate::WindowProc;
        window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;

        const ATOM atom = RegisterClass(&window_class);
        if (atom == 0)
        {
            RNDR_LOG_ERROR("Failed to register window class!");
            return;
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
    HWND window_handle =
        CreateWindowEx(0, class_name, L"Dummy name", window_style, CW_USEDEFAULT, CW_USEDEFAULT, window_rect.right - window_rect.left,
                       window_rect.bottom - window_rect.top, nullptr, nullptr, instance, this);
    if (window_handle == nullptr)
    {
        RNDR_LOG_ERROR("CreateWindowEx failed!");
        return;
    }

    // Setup raw input
    constexpr uint16_t k_hid_usage_page_generic = 0x01;
    constexpr uint16_t k_hid_usage_generic_mouse = 0x02;
    Opal::InPlaceArray<RAWINPUTDEVICE, 1> raw_devices;
    raw_devices[0].usUsagePage = k_hid_usage_page_generic;
    raw_devices[0].usUsage = k_hid_usage_generic_mouse;
    raw_devices[0].dwFlags = RIDEV_INPUTSINK;
    raw_devices[0].hwndTarget = window_handle;
    RegisterRawInputDevices(raw_devices.GetData(), 1, sizeof(raw_devices[0]));

    m_handle = reinterpret_cast<NativeWindowHandle>(window_handle);
    if (!SetCursorMode(desc.cursor_mode))
    {
        DestroyWindow(window_handle);
        m_handle = nullptr;
        RNDR_LOG_ERROR("Failed to set cursor mode!");
        return;
    }

    // At this point we have a valid window
    m_desc = desc;
    m_handle = reinterpret_cast<NativeWindowHandle>(window_handle);
    m_is_visible = desc.start_visible;
    m_is_minimized = desc.start_minimized;
    m_is_maximized = desc.start_maximized;
    m_is_closed = false;
    RNDR_LOG_INFO("Window created successfully!");
}

Rndr::Window::~Window()
{
    Destroy();
}

Rndr::Window::Window(Rndr::Window&& other) noexcept
    : m_desc(other.m_desc),
      m_handle(other.m_handle),
      m_is_visible(other.m_is_visible),
      m_is_minimized(other.m_is_minimized),
      m_is_maximized(other.m_is_maximized),
      m_is_closed(other.m_is_closed)
{
    if (this == &other)
    {
        return;
    }
    other.m_handle = nullptr;
    other.m_is_closed = true;
}

Rndr::Window& Rndr::Window::operator=(Rndr::Window&& other) noexcept
{
    if (this != &other)
    {
        if (m_handle != nullptr)
        {
            const BOOL status = DestroyWindow(reinterpret_cast<HWND>(m_handle));
            if (status == 0)
            {
                RNDR_LOG_ERROR("Failed to destroy window!");
            }
        }

        m_desc = other.m_desc;
        m_handle = other.m_handle;
        m_is_visible = other.m_is_visible;
        m_is_minimized = other.m_is_minimized;
        m_is_maximized = other.m_is_maximized;
        m_is_closed = other.m_is_closed;

        other.m_handle = nullptr;
        other.m_is_closed = true;
    }
    return *this;
}

void Rndr::Window::Destroy()
{
    if (m_handle == nullptr)
    {
        return;
    }
    const BOOL status = DestroyWindow(reinterpret_cast<HWND>(m_handle));
    if (status == 0)
    {
        RNDR_LOG_ERROR("Failed to destroy window!");
    }
}

void Rndr::Window::ProcessEvents() const
{
    RNDR_CPU_EVENT_SCOPED("Window::ProcessEvents");

    if (m_handle == nullptr)
    {
        RNDR_LOG_WARNING("This window can't process events since it is not valid!");
        return;
    }

    MSG msg;
    while (PeekMessage(&msg, reinterpret_cast<HWND>(m_handle), 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Rndr::Window::Close() const
{
    if (!m_is_closed)
    {
        PostMessage(reinterpret_cast<HWND>(m_handle), WM_CLOSE, 0, 0);
    }
}

bool Rndr::Window::IsClosed() const
{
    return m_is_closed;
}

Rndr::NativeWindowHandle Rndr::Window::GetNativeWindowHandle() const
{
    return m_handle;
}

int Rndr::Window::GetWidth() const
{
    return m_desc.width;
}

int Rndr::Window::GetHeight() const
{
    return m_desc.height;
}

Rndr::Vector2f Rndr::Window::GetSize() const
{
    Vector2f size;
    size.x = static_cast<float>(m_desc.width);
    size.y = static_cast<float>(m_desc.height);
    return size;
}

bool Rndr::Window::Resize(int width, int height) const
{
    if (width <= 0 || height <= 0)
    {
        RNDR_LOG_ERROR("Invalid window size!");
        return false;
    }
    if (m_handle == nullptr)
    {
        RNDR_LOG_ERROR("Invalid window handle!");
        return false;
    }

    // Define the desired client area size
    RECT rc = {0, 0, width, height};

    // Calculate the required window size, including the frame and borders
    DWORD const dw_style = GetWindowLong(reinterpret_cast<HWND>(m_handle), GWL_STYLE);
    DWORD const dw_ex_style = GetWindowLong(reinterpret_cast<HWND>(m_handle), GWL_EXSTYLE);
    BOOL const has_menu = static_cast<const BOOL>(GetMenu(reinterpret_cast<HWND>(m_handle)) != nullptr);
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
    status = SetWindowPos(reinterpret_cast<HWND>(m_handle), nullptr, 0, 0, new_width, new_height, flags);
    if (status == 0)
    {
        RNDR_LOG_ERROR("Failed to set window position!");
        return false;
    }
    return true;
}

bool Rndr::Window::IsWindowMinimized() const
{
    return m_is_minimized;
}

void Rndr::Window::SetMinimized(bool should_minimize) const
{
    if (m_handle == nullptr)
    {
        RNDR_LOG_ERROR("Invalid window handle!");
        return;
    }
    PostMessage(reinterpret_cast<HWND>(m_handle), WM_SYSCOMMAND, should_minimize ? SC_MINIMIZE : SC_RESTORE, 0);
}

bool Rndr::Window::IsWindowMaximized() const
{
    return m_is_maximized;
}

void Rndr::Window::SetMaximized(bool should_maximize) const
{
    if (m_handle == nullptr)
    {
        RNDR_LOG_ERROR("Invalid window handle!");
        return;
    }
    PostMessage(reinterpret_cast<HWND>(m_handle), WM_SYSCOMMAND, should_maximize ? SC_MAXIMIZE : SC_RESTORE, 0);
}

void Rndr::Window::SetVisible(bool visible)
{
    if (m_handle == nullptr)
    {
        RNDR_LOG_ERROR("Invalid window handle!");
        return;
    }
    ShowWindow(reinterpret_cast<HWND>(m_handle), visible ? SW_SHOW : SW_HIDE);
    m_is_visible = visible;
}

bool Rndr::Window::IsVisible() const
{
    return m_is_visible && !m_is_minimized;
}

namespace
{
bool IsCursorHidden();
POINT GetWindowMidPointInScreenSpace(HWND window_handle);
}  // namespace

bool Rndr::Window::SetCursorMode(CursorMode mode)
{
    m_desc.cursor_mode = mode;
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
            GetWindowRect(reinterpret_cast<HWND>(m_handle), &window_rect);
            ClipCursor(&window_rect);
            if (!IsCursorHidden())
            {
                ShowCursor(FALSE);
            }
            const POINT mid_point = GetWindowMidPointInScreenSpace(reinterpret_cast<HWND>(m_handle));
            SetCursorPos(mid_point.x, mid_point.y);
            break;
        }
    }

    return true;
}

Rndr::CursorMode Rndr::Window::GetCursorMode() const
{
    return m_desc.cursor_mode;
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

POINT GetWindowMidPointInScreenSpace(HWND window_handle)
{
    RECT window_rect;
    GetWindowRect(window_handle, &window_rect);
    const int width = window_rect.right - window_rect.left;
    const int height = window_rect.bottom - window_rect.top;
    const int mid_x = window_rect.left + width / 2;
    const int mid_y = window_rect.top + height / 2;
    return {mid_x, mid_y};
}

Rndr::InputPrimitive GetPrimitive(UINT msg_code)
{
    switch (msg_code)
    {
        case WM_LBUTTONDOWN:
            [[fallthrough]];
        case WM_LBUTTONUP:
            [[fallthrough]];
        case WM_LBUTTONDBLCLK:
            return Rndr::InputPrimitive::Mouse_LeftButton;
        case WM_RBUTTONDOWN:
            [[fallthrough]];
        case WM_RBUTTONUP:
            [[fallthrough]];
        case WM_RBUTTONDBLCLK:
            return Rndr::InputPrimitive::Mouse_RightButton;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
            return Rndr::InputPrimitive::Mouse_MiddleButton;
        default:
            RNDR_ASSERT(false, "Unsupported message code!");
    }
    return Rndr::InputPrimitive::Mouse_AxisWheel;
}

Rndr::InputTrigger GetTrigger(UINT msg_code)
{
    switch (msg_code)
    {
        case WM_LBUTTONDOWN:
            [[fallthrough]];
        case WM_RBUTTONDOWN:
            [[fallthrough]];
        case WM_MBUTTONDOWN:
            return Rndr::InputTrigger::ButtonPressed;
        case WM_LBUTTONUP:
            [[fallthrough]];
        case WM_RBUTTONUP:
            [[fallthrough]];
        case WM_MBUTTONUP:
            return Rndr::InputTrigger::ButtonReleased;
        case WM_LBUTTONDBLCLK:
            [[fallthrough]];
        case WM_RBUTTONDBLCLK:
            [[fallthrough]];
        case WM_MBUTTONDBLCLK:
            return Rndr::InputTrigger::ButtonDoubleClick;
        default:
            RNDR_ASSERT(false, "Unsupported message code!");
    }
    return Rndr::InputTrigger::ButtonPressed;
}
}  // namespace

namespace Rndr::WindowPrivate
{
void HandleMouseMove(Rndr::Window* window, int x, int y)
{
    if (window == nullptr)
    {
        return;
    }

    // We need to flip Y since the engine expects Y to grow from bottom to top
    y = window->m_desc.height - y;

    const Rndr::CursorMode mode = window->m_desc.cursor_mode;
    switch (mode)
    {
        case Rndr::CursorMode::Normal:
        case Rndr::CursorMode::Hidden:
        {
            const Point2f absolute_position(static_cast<float>(x), static_cast<float>(y));
            Rndr::InputSystem::SubmitMousePositionEvent(window->m_handle, absolute_position, window->GetSize());
            break;
        }
        case Rndr::CursorMode::Infinite:
        {
            const POINT mid_point = GetWindowMidPointInScreenSpace(reinterpret_cast<HWND>(window->m_handle));
            SetCursorPos(mid_point.x, mid_point.y);
            break;
        }
    }
}

LRESULT CALLBACK WindowProc(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l)
{
    LONG_PTR const window_ptr = GetWindowLongPtr(window_handle, GWLP_USERDATA);
    // Before window is created this will be nullptr
    Rndr::Window* window = reinterpret_cast<Rndr::Window*>(window_ptr);  // NOLINT

    if (window != nullptr)
    {
        if (window->on_native_event.IsBound())
        {
            const bool status = window->on_native_event.Execute(window_handle, msg_code, param_w, param_l);
            if (status)
            {
                return DefWindowProc(window_handle, msg_code, param_w, param_l);
            }
        }
    }

    switch (msg_code)
    {
        case WM_CREATE:
        {
            RNDR_LOG_DEBUG("WindowProc: Event WM_CREATE");
            CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(param_l);  // NOLINT
            window = reinterpret_cast<Rndr::Window*>(CreateStruct->lpCreateParams);
            SetWindowLongPtr(window_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
            break;
        }
        case WM_SIZE:
        {
            if (window == nullptr)
            {
                break;
            }
            const int32_t width = static_cast<int32_t>(LOWORD(param_l));
            const int32_t height = static_cast<int32_t>(HIWORD(param_l));
            RNDR_LOG_DEBUG("WindowProc: Event WM_SIZE (%d, %d)", width, height);
            window->m_desc.width = width;
            window->m_desc.height = height;
            window->on_resize.Execute(width, height);
            break;
        }
        case WM_CLOSE:
        {
            RNDR_LOG_DEBUG("WindowProc: Event WM_CLOSE");
            window->m_is_closed = true;
            break;
        }
        case WM_DESTROY:
        {
            RNDR_LOG_DEBUG("WindowProc: Event WM_DESTROY");
            window->m_handle = nullptr;
            break;
        }
        case WM_QUIT:
        {
            RNDR_LOG_DEBUG("WindowProc: Event WM_QUIT");
            break;
        }
        case WM_SYSCOMMAND:
        {
            const uint16_t mask = param_w & 0xFFF0;
            if (mask == SC_MINIMIZE)
            {
                RNDR_LOG_DEBUG("WindowProc: Event SC_MINIMIZE");
                window->m_is_minimized = true;
                window->m_is_maximized = false;
                break;
            }
            if (mask == SC_MAXIMIZE)
            {
                RNDR_LOG_DEBUG("WindowProc: Event SC_MAXIMIZE");
                window->m_is_minimized = false;
                window->m_is_maximized = true;
                break;
            }
            if (mask == SC_RESTORE)
            {
                RNDR_LOG_DEBUG("WindowProc: Event SC_RESTORE");
                window->m_is_minimized = false;
                window->m_is_maximized = false;
                break;
            }
            break;
        }
        case WM_MOUSEMOVE:
        {
            const int x = GET_X_LPARAM(param_l);
            const int y = GET_Y_LPARAM(param_l);
            HandleMouseMove(window, x, y);
            break;
        }
        case WM_INPUT:
        {
            // Handling infinite mouse cursor mode
            if (window->m_desc.cursor_mode != Rndr::CursorMode::Infinite)
            {
                break;
            }

            UINT struct_size = sizeof(RAWINPUT);
            Opal::InPlaceArray<uint8_t, sizeof(RAWINPUT)> data_buffer;

            // NOLINTNEXTLINE
            GetRawInputData(reinterpret_cast<HRAWINPUT>(param_l), RID_INPUT, data_buffer.GetData(), &struct_size, sizeof(RAWINPUTHEADER));

            RAWINPUT* raw_data = reinterpret_cast<RAWINPUT*>(data_buffer.GetData());

            if (raw_data->header.dwType == RIM_TYPEMOUSE)
            {
                const Vector2f size = window->GetSize();
                const Vector2f delta{static_cast<float>(raw_data->data.mouse.lLastX) / size.x,
                                     static_cast<float>(raw_data->data.mouse.lLastY) / size.y};
                Rndr::InputSystem::SubmitRelativeMousePositionEvent(window->m_handle, delta, size);
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
            if (window == nullptr)
            {
                break;
            }

            const Rndr::InputPrimitive primitive = GetPrimitive(msg_code);
            const Rndr::InputTrigger trigger = GetTrigger(msg_code);
            Rndr::InputSystem::SubmitButtonEvent(window->m_handle, primitive, trigger);
            break;
        }
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            if (window == nullptr)
            {
                break;
            }
            const auto iter = g_vk_to_primitive.find(static_cast<uint32_t>(param_w));
            if (iter == g_vk_to_primitive.end())
            {
                break;
            }
            const Rndr::InputPrimitive primitive = iter->second;
            const Rndr::InputTrigger trigger =
                msg_code == WM_KEYDOWN ? Rndr::InputTrigger::ButtonPressed : Rndr::InputTrigger::ButtonReleased;
            Rndr::InputSystem::SubmitButtonEvent(window->m_handle, primitive, trigger);
            break;
        }
        case WM_MOUSEWHEEL:
        {
            if (window == nullptr)
            {
                break;
            }
            const int delta_wheel = GET_WHEEL_DELTA_WPARAM(param_w);
            Rndr::InputSystem::SubmitMouseWheelEvent(window->m_handle, delta_wheel);
            break;
        }
    }

    return DefWindowProc(window_handle, msg_code, param_w, param_l);
}
}  // namespace Rndr::WindowPrivate
