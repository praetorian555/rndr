#include "rndr/platform/windows-window.hpp"

#include "opal/allocator.h"
#include "opal/container/in-place-array.h"
#include "opal/container/string.h"

#include "rndr/platform/windows-application.hpp"
#include "rndr/platform/windows-header.hpp"

#include "rndr/log.hpp"

Rndr::WindowsWindow::WindowsWindow(const GenericWindowDesc& desc) : GenericWindow(desc) {}

Opal::Expected<Opal::ScopePtr<Rndr::GenericWindow>, Rndr::ErrorCode> Rndr::WindowsWindow::Create(const GenericWindowDesc& desc)
{
    using ResultType = Opal::Expected<Opal::ScopePtr<GenericWindow>, ErrorCode>;
    if (desc.width == 0 || desc.height == 0)
    {
        RNDR_LOG_ERROR("Window width and height must be greater than 0");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (desc.start_minimized && desc.start_maximized)
    {
        RNDR_LOG_ERROR("Window cannot be both minimized and maximized at the same time");
        return ResultType(ErrorCode::InvalidArgument);
    }

    Opal::ScopePtr<GenericWindow> window = Opal::MakeScoped<GenericWindow, WindowsWindow>(Opal::GetDefaultAllocator(), desc);
    const ErrorCode err = static_cast<WindowsWindow*>(window.Get())->Initialize(desc);
    if (err != ErrorCode::Success)
    {
        return ResultType(err);
    }
    return ResultType(std::move(window));
}

Rndr::ErrorCode Rndr::WindowsWindow::Initialize(const GenericWindowDesc& desc)
{
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
            RNDR_LOG_ERROR("Failed to register window class");
            return ErrorCode::PlatformError;
        }
    }

    const DWORD window_style = static_cast<DWORD>(GetWindowedStyle(desc) | GetInitialStateStyle(desc));
    const DWORD extended_style = static_cast<DWORD>(GetExtendedStyle(desc));

    // Since the user specifies the size of the client area but CreateWindowEx expects the size of the whole window,
    // we will ask OS how big should the window be for the desired client area.
    RECT rc = {0, 0, desc.width, desc.height};
    ::AdjustWindowRectEx(&rc, window_style, FALSE, extended_style);
    const i32 real_width = rc.right - rc.left;
    const i32 real_height = rc.bottom - rc.top;

    Opal::StringUtf8 name = desc.name;
    Opal::StringWide wide_name(name.GetSize() + 1, 0);
    Opal::Transcode(name, wide_name);
    HWND window_handle = CreateWindowEx(extended_style, class_name, wide_name.GetData(), window_style, desc.start_x, desc.start_y,
                                        real_width, real_height, nullptr, nullptr, instance, this);
    if (window_handle == nullptr)
    {
        RNDR_LOG_ERROR("CreateWindowEx failed");
        return ErrorCode::PlatformError;
    }

    m_native_window_handle = reinterpret_cast<NativeWindowHandle>(window_handle);
    ApplyCloseSupport();
    if (desc.always_on_top)
    {
        ::SetWindowPos(window_handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    m_pos_x = desc.start_x;
    m_pos_y = desc.start_y;
    m_width = desc.width;
    m_height = desc.height;
    m_dpi_scale = static_cast<f32>(::GetDpiForWindow(window_handle)) / 96.0f;

    ::SetActiveWindow(window_handle);

    RNDR_LOG_INFO("Window created successfully!");
    return ErrorCode::Success;
}

Rndr::WindowsWindow::~WindowsWindow()
{
    WindowsWindow::Destroy();
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
        RNDR_LOG_ERROR("PostMessage(WM_CLOSE) failed");
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::WindowsWindow::Reshape(i32 pos_x, i32 pos_y, i32 width, i32 height)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    m_pos_x = pos_x;
    m_pos_y = pos_y;
    m_width = width;
    m_height = height;
    const BOOL rtn = MoveWindow(RNDR_TO_HWND(m_native_window_handle), pos_x, pos_y, width, height, TRUE);
    if (rtn == 0)
    {
        RNDR_LOG_ERROR("MoveWindow failed");
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
    m_pos_x = pos_x;
    m_pos_y = pos_y;
    const BOOL rtn = MoveWindow(RNDR_TO_HWND(m_native_window_handle), pos_x, pos_y, m_width, m_height, TRUE);
    if (rtn == 0)
    {
        RNDR_LOG_ERROR("MoveWindow failed");
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

void Rndr::WindowsWindow::BringToFront()
{
    if (m_is_closed)
    {
        return;
    }
    if (IsMinimized())
    {
        Restore();
    }
    SetActiveWindow(RNDR_TO_HWND(m_native_window_handle));
}

void Rndr::WindowsWindow::Destroy()
{
    if (m_native_window_handle == nullptr)
    {
        return;
    }
    DestroyWindow(RNDR_TO_HWND(m_native_window_handle));
    m_native_window_handle = nullptr;
}

void Rndr::WindowsWindow::Minimize()
{
    if (m_is_closed)
    {
        return;
    }
    ShowWindow(RNDR_TO_HWND(m_native_window_handle), SW_MINIMIZE);
}

void Rndr::WindowsWindow::Maximize()
{
    if (m_is_closed)
    {
        return;
    }
    ShowWindow(RNDR_TO_HWND(m_native_window_handle), SW_MAXIMIZE);
}

void Rndr::WindowsWindow::Restore()
{
    if (m_is_closed)
    {
        return;
    }
    ShowWindow(RNDR_TO_HWND(m_native_window_handle), SW_RESTORE);
}

void Rndr::WindowsWindow::Enable(bool enable)
{
    if (m_is_closed)
    {
        return;
    }
    EnableWindow(RNDR_TO_HWND(m_native_window_handle), static_cast<BOOL>(enable));
}

void Rndr::WindowsWindow::Show()
{
    if (m_is_closed)
    {
        return;
    }
    ShowWindow(RNDR_TO_HWND(m_native_window_handle), SW_SHOW);
}

void Rndr::WindowsWindow::Hide()
{
    if (m_is_closed)
    {
        return;
    }
    ShowWindow(RNDR_TO_HWND(m_native_window_handle), SW_HIDE);
}

void Rndr::WindowsWindow::Focus()
{
    if (m_is_closed)
    {
        return;
    }
    SetFocus(RNDR_TO_HWND(m_native_window_handle));
}

Rndr::i32 Rndr::WindowsWindow::GetWindowedStyle(const GenericWindowDesc& desc)
{
    // WS_OVERLAPPED - means that the window has a title bar and a border.
    // WS_CAPTION - same as WS_OVERLAPPED.
    // WS_SYSMENU - has the window menu on the title bar, required for the caption buttons.
    // WS_POPUP - window without a title bar.
    // WS_THICKFRAME - window can be resized.
    // WS_BORDER - window has a border.
    i32 window_style = desc.has_title_bar ? (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU) : WS_POPUP;
    if (desc.resizable)
    {
        // A resizable window always needs a sizing frame, no matter what has_border says.
        window_style |= WS_THICKFRAME;
    }
    else if (desc.has_border)
    {
        window_style |= WS_BORDER;
    }
    // The caption buttons only exist together with the title bar. Keeping the box styles off without one avoids
    // a non-client area that the OS reserves but never draws.
    if (desc.has_title_bar)
    {
        window_style |= desc.supports_maximize ? WS_MAXIMIZEBOX : 0;
        window_style |= desc.supports_minimize ? WS_MINIMIZEBOX : 0;
    }
    return window_style;
}

Rndr::i32 Rndr::WindowsWindow::GetInitialStateStyle(const GenericWindowDesc& desc)
{
    i32 window_style = 0;
    window_style |= desc.start_minimized ? WS_MINIMIZE : 0;
    window_style |= desc.start_maximized ? WS_MAXIMIZE : 0;
    window_style |= desc.start_visible ? WS_VISIBLE : 0;
    return window_style;
}

Rndr::i32 Rndr::WindowsWindow::GetExtendedStyle(const GenericWindowDesc& desc)
{
    // WS_EX_LAYERED - window can be made translucent through SetLayeredWindowAttributes.
    // WS_EX_TOOLWINDOW - window is left out of the task bar and the Alt+Tab list.
    // WS_EX_APPWINDOW - window gets a task bar button.
    // WS_EX_TOPMOST - listed here for AdjustWindowRectEx, the actual z-order is set through SetWindowPos.
    i32 extended_style = 0;
    extended_style |= desc.supports_transparency ? WS_EX_LAYERED : 0;
    extended_style |= desc.show_in_taskbar ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW;
    extended_style |= desc.always_on_top ? WS_EX_TOPMOST : 0;
    return extended_style;
}

Rndr::i32 Rndr::WindowsWindow::GetFullscreenStyle(const GenericWindowDesc& desc)
{
    RNDR_UNUSED(desc);
    return WS_POPUP;
}

void Rndr::WindowsWindow::ApplyStyle()
{
    if (m_is_closed || m_native_window_handle == nullptr)
    {
        return;
    }
    const HWND hwnd = RNDR_TO_HWND(m_native_window_handle);

    // Keep whatever state the window is in right now, the desc only describes the state it started in.
    const LONG current_style = ::GetWindowLong(hwnd, GWL_STYLE);
    LONG new_style = m_mode == GenericWindowMode::Windowed ? GetWindowedStyle(m_desc) : GetFullscreenStyle(m_desc);
    new_style |= current_style & (WS_VISIBLE | WS_MINIMIZE | WS_MAXIMIZE);

    // SetOpacity can turn the window into a layered one on demand, so never drop that bit here.
    const LONG current_extended_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
    const LONG new_extended_style = GetExtendedStyle(m_desc) | (current_extended_style & WS_EX_LAYERED);

    ::SetWindowLong(hwnd, GWL_STYLE, new_style);
    ::SetWindowLong(hwnd, GWL_EXSTYLE, new_extended_style);
    ApplyCloseSupport();

    // SWP_FRAMECHANGED makes the OS recompute the non-client area, without it the old frame stays on screen.
    HWND insert_after = m_desc.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST;
    ::SetWindowPos(hwnd, insert_after, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

void Rndr::WindowsWindow::ApplyCloseSupport()
{
    if (m_native_window_handle == nullptr)
    {
        return;
    }
    // Graying out SC_CLOSE disables the close button and Alt+F4. It does not block WM_CLOSE, so RequestClose keeps working.
    HMENU system_menu = ::GetSystemMenu(RNDR_TO_HWND(m_native_window_handle), FALSE);
    if (system_menu == nullptr)
    {
        return;
    }
    const UINT flags = m_desc.supports_close ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_DISABLED | MF_GRAYED;
    ::EnableMenuItem(system_menu, SC_CLOSE, flags);
}

void Rndr::WindowsWindow::SetMode(GenericWindowMode mode)
{
    if (m_is_closed)
    {
        return;
    }
    if (m_mode == mode)
    {
        return;
    }
    const HWND hwnd = RNDR_TO_HWND(m_native_window_handle);
    m_mode = mode;
    if (m_mode == GenericWindowMode::Windowed)
    {
        ApplyStyle();
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

        ApplyStyle();
        SetWindowPos(hwnd, nullptr, monitor_info.rcMonitor.left, monitor_info.rcMonitor.top, monitor_width, monitor_height,
                     SWP_NOZORDER | SWP_FRAMECHANGED);

        ::ShowWindow(hwnd, SW_RESTORE);
    }
}

void Rndr::WindowsWindow::SetResizable(bool resizable)
{
    if (m_desc.resizable == resizable)
    {
        return;
    }
    m_desc.resizable = resizable;
    ApplyStyle();
}

void Rndr::WindowsWindow::SetTitleBarVisible(bool visible)
{
    if (m_desc.has_title_bar == visible)
    {
        return;
    }
    m_desc.has_title_bar = visible;
    ApplyStyle();
}

void Rndr::WindowsWindow::SetBorderVisible(bool visible)
{
    if (m_desc.has_border == visible)
    {
        return;
    }
    m_desc.has_border = visible;
    ApplyStyle();
}

void Rndr::WindowsWindow::SetMinimizeSupported(bool supported)
{
    if (m_desc.supports_minimize == supported)
    {
        return;
    }
    m_desc.supports_minimize = supported;
    ApplyStyle();
}

void Rndr::WindowsWindow::SetMaximizeSupported(bool supported)
{
    if (m_desc.supports_maximize == supported)
    {
        return;
    }
    m_desc.supports_maximize = supported;
    ApplyStyle();
}

void Rndr::WindowsWindow::SetCloseSupported(bool supported)
{
    if (m_desc.supports_close == supported)
    {
        return;
    }
    m_desc.supports_close = supported;
    ApplyCloseSupport();
    // The close button is painted as part of the non-client area, so ask for a redraw of the frame.
    if (!m_is_closed && m_native_window_handle != nullptr)
    {
        ::SetWindowPos(RNDR_TO_HWND(m_native_window_handle), nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }
}

void Rndr::WindowsWindow::SetVisibleInTaskbar(bool visible)
{
    if (m_desc.show_in_taskbar == visible)
    {
        return;
    }
    m_desc.show_in_taskbar = visible;
    if (m_is_closed || m_native_window_handle == nullptr)
    {
        return;
    }
    // The shell only re-evaluates the task bar button when the window is shown, so a visible window has to be
    // cycled for the new extended style to take effect.
    const HWND hwnd = RNDR_TO_HWND(m_native_window_handle);
    const bool was_visible = IsVisible();
    if (was_visible)
    {
        ::ShowWindow(hwnd, SW_HIDE);
    }
    ApplyStyle();
    if (was_visible)
    {
        ::ShowWindow(hwnd, SW_SHOW);
    }
}

void Rndr::WindowsWindow::SetAlwaysOnTop(bool always_on_top)
{
    if (m_desc.always_on_top == always_on_top)
    {
        return;
    }
    m_desc.always_on_top = always_on_top;
    ApplyStyle();
}

Rndr::ErrorCode Rndr::WindowsWindow::SetOpacity(f32 opacity)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    // SetLayeredWindowAttributes only works on layered windows, so turn this one into one if it is not already.
    const HWND hwnd = RNDR_TO_HWND(m_native_window_handle);
    const LONG extended_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
    if ((extended_style & WS_EX_LAYERED) == 0)
    {
        ::SetWindowLong(hwnd, GWL_EXSTYLE, extended_style | WS_EX_LAYERED);
        m_desc.supports_transparency = true;
    }
    const BYTE alpha = static_cast<BYTE>(opacity * 255);
    const BOOL rtn = SetLayeredWindowAttributes(RNDR_TO_HWND(m_native_window_handle), 0, alpha, LWA_ALPHA);
    if (rtn == 0)
    {
        RNDR_LOG_ERROR("SetLayeredWindowAttributes failed");
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
        RNDR_LOG_ERROR("Failed to transcode title string");
        return ErrorCode::InvalidArgument;
    }
    const BOOL rtn = SetWindowTextW(RNDR_TO_HWND(m_native_window_handle), win_string.GetData());
    if (rtn == 0)
    {
        RNDR_LOG_ERROR("SetWindowTextW failed");
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
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

bool Rndr::WindowsWindow::IsBorderlessFullscreen() const
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

void Rndr::WindowsWindow::EnableHighPrecisionCursorMode(bool enable)
{
    HWND window_handle = nullptr;
    if (enable)
    {
        window_handle = RNDR_TO_HWND(m_native_window_handle);
    }

    constexpr uint16_t k_hid_usage_page_generic = 0x01;
    constexpr uint16_t k_hid_usage_generic_mouse = 0x02;
    Opal::InPlaceArray<RAWINPUTDEVICE, 1> raw_devices;
    raw_devices[0].usUsagePage = k_hid_usage_page_generic;
    raw_devices[0].usUsage = k_hid_usage_generic_mouse;
    raw_devices[0].dwFlags = RIDEV_INPUTSINK;
    raw_devices[0].hwndTarget = window_handle;
    RegisterRawInputDevices(raw_devices.GetData(), 1, sizeof(raw_devices[0]));
    m_high_precision_cursor = enable;
}

bool Rndr::WindowsWindow::IsHighPrecisionCursorModeEnabled() const
{
    return m_high_precision_cursor;
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

Rndr::Vector2i Rndr::WindowsWindow::GetPosition() const
{
    RECT window_rect = {};
    GetWindowRect(RNDR_TO_HWND(m_native_window_handle), &window_rect);
    return {window_rect.left, window_rect.top};
}

Rndr::Vector2i Rndr::WindowsWindow::GetSize() const
{
    RECT window_rect = {};
    ::GetClientRect(RNDR_TO_HWND(m_native_window_handle), &window_rect);
    return {window_rect.right - window_rect.left, window_rect.bottom - window_rect.top};
}

Rndr::GenericWindowMode Rndr::WindowsWindow::GetMode() const
{
    return m_mode;
}

Rndr::NativeWindowHandle Rndr::WindowsWindow::GetNativeHandle() const
{
    return m_native_window_handle;
}
