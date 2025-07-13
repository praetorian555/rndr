#include "rndr/platform/windows-window.hpp"

#include "opal/container/string.h"

#include "rndr/platform/windows-application.hpp"
#include "rndr/platform/windows-header.hpp"

#include "rndr/log.hpp"

Rndr::WindowsWindow::WindowsWindow(const GenericWindowDesc& desc, Opal::AllocatorBase* allocator) : GenericWindow(desc, allocator)
{
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

    WNDCLASS window_class{};
    if (!GetClassInfo(instance, class_name, &window_class))
    {
        window_class.lpszClassName = class_name;
        window_class.hInstance = instance;
        window_class.lpfnWndProc = RndrPrivate::WindowProc;
        window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;

        const ATOM atom = RegisterClass(&window_class);
        if (atom == 0)
        {
            RNDR_LOG_ERROR("Failed to register window class!");
            return;
        }
    }

    const DWORD window_style = GetWindowedStyle(desc);

    // Since the user specifies the size of the client area but CreateWindowEx expects the size of the whole window,
    // we will ask OS how big should the window be for the desired client area.
    RECT rc = {0, 0, desc.width, desc.height};
    ::AdjustWindowRectEx(&rc, window_style, FALSE, 0);
    const i32 real_width = rc.right - rc.left;
    const i32 real_height = rc.bottom - rc.top;

    Opal::StringUtf8 name = desc.name;
    Opal::StringWide wide_name(name.GetSize() + 1, 0);
    Opal::Transcode(name, wide_name);
    HWND window_handle = CreateWindowEx(0, class_name, wide_name.GetData(), window_style, desc.start_x, desc.start_y, real_width,
                                        real_height, nullptr, nullptr, instance, this);
    if (window_handle == nullptr)
    {
        RNDR_LOG_ERROR("CreateWindowEx failed!");
        return;
    }

    m_native_window_handle = reinterpret_cast<NativeWindowHandle>(window_handle);
    m_pos_x = desc.start_x;
    m_pos_y = desc.start_y;
    m_width = desc.width;
    m_height = desc.height;

    ::SetActiveWindow(window_handle);

    RNDR_LOG_INFO("Window created successfully!");
}

Rndr::ErrorCode Rndr::WindowsWindow::RequestClose()
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    const BOOL rtn = PostMessage(reinterpret_cast<HWND>(m_native_window_handle), WM_CLOSE, 0, 0);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::ForceClose()
{
    m_is_closed = true;
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::Reshape(i32 pos_x, i32 pos_y, i32 width, i32 height)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    if (width <= 0 || height <= 0)
    {
        return ErrorCode::InvalidArgument;
    }
    if (pos_x < 0 || pos_y < 0)
    {
        return ErrorCode::OutOfBounds;
    }
    m_pos_x = pos_x;
    m_pos_y = pos_y;
    m_width = width;
    m_height = height;
    const BOOL rtn = MoveWindow(RNDR_TO_HWND(m_native_window_handle), pos_x, pos_y, width, height, TRUE);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::MoveTo(i32 pos_x, i32 pos_y)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    if (pos_x < 0 || pos_y < 0)
    {
        return ErrorCode::OutOfBounds;
    }
    m_pos_x = pos_x;
    m_pos_y = pos_y;
    const BOOL rtn = MoveWindow(RNDR_TO_HWND(m_native_window_handle), pos_x, pos_y, m_width, m_height, TRUE);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::BringToFront()
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    if (IsMinimized())
    {
        return Restore();
    }
    const HWND prev_active = SetActiveWindow(RNDR_TO_HWND(m_native_window_handle));
    if (prev_active == nullptr)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::Destroy()
{
    if (m_native_window_handle == nullptr)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    const BOOL rtn = DestroyWindow(RNDR_TO_HWND(m_native_window_handle));
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    m_native_window_handle = nullptr;
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::Minimize()
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    const BOOL rtn = ShowWindow(RNDR_TO_HWND(m_native_window_handle), SW_MINIMIZE);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::Maximize()
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    const BOOL rtn = ShowWindow(RNDR_TO_HWND(m_native_window_handle), SW_MAXIMIZE);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::Restore()
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    const BOOL rtn = ShowWindow(RNDR_TO_HWND(m_native_window_handle), SW_RESTORE);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::Enable(bool enable)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    const BOOL rtn = EnableWindow(RNDR_TO_HWND(m_native_window_handle), static_cast<BOOL>(enable));
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::Show()
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    const BOOL rtn = ShowWindow(RNDR_TO_HWND(m_native_window_handle), SW_SHOW);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::Hide()
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    const BOOL rtn = ShowWindow(RNDR_TO_HWND(m_native_window_handle), SW_HIDE);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::Focus()
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    HWND prev_active = SetFocus(RNDR_TO_HWND(m_native_window_handle));
    if (prev_active == nullptr)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::i32 Rndr::WindowsWindow::GetWindowedStyle(const GenericWindowDesc& desc)
{
    // WS_OVERLAPPED - means that the window has a title bar and a border.
    // WS_CAPTION - same as WS_OVERLAPPED.
    // WS_SYSMENU - has the window menu on the title bar.
    // WS_THICKFRAME - window can be resized.
    // WS_BORDER - window has a border.
    i32 window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    window_style |= desc.resizable ? WS_THICKFRAME : WS_BORDER;
    window_style |= desc.supports_maximize ? WS_MAXIMIZEBOX : 0;
    window_style |= desc.supports_minimize ? WS_MINIMIZEBOX : 0;
    window_style |= desc.supports_transparency ? WS_EX_LAYERED : 0;
    window_style |= desc.start_minimized ? WS_MINIMIZE : 0;
    window_style |= desc.start_maximized ? WS_MAXIMIZE : 0;
    window_style |= desc.start_visible ? WS_VISIBLE : 0;
    return window_style;
}

Rndr::i32 Rndr::WindowsWindow::GetFullscreenStyle(const GenericWindowDesc& desc)
{
    RNDR_UNUSED(desc);
    return WS_POPUP;
}

Rndr::ErrorCode Rndr::WindowsWindow::SetMode(GenericWindowMode mode)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    if (m_mode == mode)
    {
        return ErrorCode::Success;
    }
    const HWND hwnd = RNDR_TO_HWND(m_native_window_handle);
    m_mode = mode;
    LONG window_style = GetWindowLong(hwnd, GWL_STYLE);
    if (m_mode == GenericWindowMode::Windowed)
    {
        window_style &= ~GetFullscreenStyle(m_desc);
        window_style |= GetWindowedStyle(m_desc);
        SetWindowLong(hwnd, GWL_STYLE, window_style);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOZORDER);
        if (m_pre_fullscreen_placement.length > 0)
        {
            ::SetWindowPlacement(hwnd, &m_pre_fullscreen_placement);
        }
    }
    else
    {
        m_pre_fullscreen_placement.length = sizeof(WINDOWPLACEMENT);
        ::GetWindowPlacement(hwnd, &m_pre_fullscreen_placement);

        HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitor_info = {};
        monitor_info.cbSize = sizeof(MONITORINFO);
        ::GetMonitorInfo(monitor, &monitor_info);
        const i32 monitor_width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
        const i32 monitor_height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;

        window_style &= ~GetWindowedStyle(m_desc);
        window_style |= GetFullscreenStyle(m_desc);
        SetWindowLong(hwnd, GWL_STYLE, window_style);
        SetWindowPos(hwnd, nullptr, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top, monitor_width, monitor_height,
                     SWP_NOZORDER);

        ::ShowWindow(hwnd, SW_RESTORE);
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::SetOpacity(f32 opacity)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    if (!m_desc.supports_transparency)
    {
        return ErrorCode::FeatureNotSupported;
    }
    const BYTE alpha = static_cast<BYTE>(opacity * 255);
    const BOOL rtn = SetLayeredWindowAttributes(RNDR_TO_HWND(m_native_window_handle), 0, alpha, LWA_ALPHA);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::SetTitle(const Opal::StringUtf8& title)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    Opal::StringWide win_string(4 * title.GetSize(), '\0');
    const Opal::ErrorCode err = Opal::Transcode(title, win_string);
    if (err != Opal::ErrorCode::Success)
    {
        return ErrorCode::InvalidArgument;
    }
    const BOOL rtn = SetWindowTextW(RNDR_TO_HWND(m_native_window_handle), win_string.GetData());
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

bool Rndr::WindowsWindow::IsClosed() const
{
    return m_is_closed;
}

bool Rndr::WindowsWindow::IsMaximized() const
{
    return ::IsZoomed(RNDR_TO_HWND(m_native_window_handle)) != 0;
}

bool Rndr::WindowsWindow::IsMinimized() const
{
    return ::IsIconic(RNDR_TO_HWND(m_native_window_handle)) != 0;
}

bool Rndr::WindowsWindow::IsVisible() const
{
    return ::IsWindowVisible(RNDR_TO_HWND(m_native_window_handle)) != 0;
}

bool Rndr::WindowsWindow::IsFocused() const
{
    return ::GetFocus() == RNDR_TO_HWND(m_native_window_handle);
}

bool Rndr::WindowsWindow::IsEnabled() const
{
    return ::IsWindowEnabled(RNDR_TO_HWND(m_native_window_handle)) != 0;
}

bool Rndr::WindowsWindow::IsBorderless() const
{
    return m_mode == GenericWindowMode::BorderlessFullscreen;
}

bool Rndr::WindowsWindow::IsWindowed() const
{
    return m_mode == GenericWindowMode::Windowed;
}

bool Rndr::WindowsWindow::IsResizable() const
{
    return (::GetWindowLongPtr(RNDR_TO_HWND(m_native_window_handle), GWL_STYLE) & WS_THICKFRAME) != 0;
}

bool Rndr::WindowsWindow::IsMouseHovering() const
{
    HWND hwnd_parent = RNDR_TO_HWND(m_native_window_handle);
    POINT cursor_pos;
    if (GetCursorPos(&cursor_pos) == 0)
    {
        return false;
    }
    HWND hovered_hwnd = WindowFromPoint(cursor_pos);
    return hovered_hwnd == hwnd_parent || IsChild(hwnd_parent, hovered_hwnd) != 0;
}

Rndr::ErrorCode Rndr::WindowsWindow::GetPositionAndSize(i32& pos_x, i32& pos_y, i32& width, i32& height) const
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    // We use the position of the whole window (not only the client area)
    RECT window_rect = {};
    BOOL rtn = GetWindowRect(RNDR_TO_HWND(m_native_window_handle), &window_rect);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    pos_x = window_rect.left;
    pos_y = window_rect.top;

    // We use the size of the client area only
    rtn = ::GetClientRect(RNDR_TO_HWND(m_native_window_handle), &window_rect);
    if (rtn == 0)
    {
        return ErrorCode::PlatformError;
    }
    width = window_rect.right - window_rect.left;
    height = window_rect.bottom - window_rect.top;
    return ErrorCode::Success;
}

Opal::Expected<Rndr::Vector2i, Rndr::ErrorCode> Rndr::WindowsWindow::GetSize() const
{
    if (m_is_closed)
    {
        return Opal::Expected<Vector2i, ErrorCode>(ErrorCode::WindowAlreadyClosed);
    }
    // We use the size of the client area only
    RECT window_rect = {};
    BOOL rtn = ::GetClientRect(RNDR_TO_HWND(m_native_window_handle), &window_rect);
    if (rtn == 0)
    {
        return Opal::Expected<Vector2i, ErrorCode>(ErrorCode::PlatformError);
    }
    Vector2i size(window_rect.right - window_rect.left, window_rect.bottom - window_rect.top);
    return Opal::Expected<Vector2i, ErrorCode>(size);
}

Rndr::GenericWindowMode Rndr::WindowsWindow::GetMode() const
{
    return m_mode;
}

Rndr::NativeWindowHandle Rndr::WindowsWindow::GetNativeHandle() const
{
    return m_native_window_handle;
}
