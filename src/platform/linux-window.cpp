#include "rndr/platform/linux-window.hpp"

#if RNDR_LINUX

#include <cstdlib>
#include <ctime>

#include "opal/allocator.h"
#include "opal/container/string.h"

#include "rndr/log.hpp"
#include "rndr/platform/linux-application.hpp"

namespace
{

/** _MOTIF_WM_HINTS payload, the de-facto protocol for controlling window decorations. */
struct MotifWmHints
{
    Rndr::u32 flags;
    Rndr::u32 functions;
    Rndr::u32 decorations;
    Rndr::i32 input_mode;
    Rndr::u32 status;
};

constexpr Rndr::u32 k_motif_hints_functions = 1 << 0;
constexpr Rndr::u32 k_motif_hints_decorations = 1 << 1;

constexpr Rndr::u32 k_motif_func_resize = 1 << 1;
constexpr Rndr::u32 k_motif_func_move = 1 << 2;
constexpr Rndr::u32 k_motif_func_minimize = 1 << 3;
constexpr Rndr::u32 k_motif_func_maximize = 1 << 4;
constexpr Rndr::u32 k_motif_func_close = 1 << 5;

constexpr Rndr::u32 k_motif_decor_border = 1 << 1;
constexpr Rndr::u32 k_motif_decor_resize_handle = 1 << 2;
constexpr Rndr::u32 k_motif_decor_title = 1 << 3;
constexpr Rndr::u32 k_motif_decor_menu = 1 << 4;
constexpr Rndr::u32 k_motif_decor_minimize = 1 << 5;
constexpr Rndr::u32 k_motif_decor_maximize = 1 << 6;

/** ICCCM WM_NORMAL_HINTS payload (XSizeHints on the wire): 18 32-bit fields. */
struct WmSizeHints
{
    Rndr::u32 flags;
    Rndr::i32 x, y, width, height;
    Rndr::i32 min_width, min_height;
    Rndr::i32 max_width, max_height;
    Rndr::i32 width_inc, height_inc;
    Rndr::i32 min_aspect_num, min_aspect_den;
    Rndr::i32 max_aspect_num, max_aspect_den;
    Rndr::i32 base_width, base_height;
    Rndr::u32 win_gravity;
};

constexpr Rndr::u32 k_size_hint_p_position = 1 << 2;
constexpr Rndr::u32 k_size_hint_p_min_size = 1 << 4;
constexpr Rndr::u32 k_size_hint_p_max_size = 1 << 5;

/** ICCCM WM_HINTS payload: 9 32-bit fields. */
struct WmHints
{
    Rndr::u32 flags;
    Rndr::u32 input;
    Rndr::u32 initial_state;
    Rndr::u32 icon_pixmap;
    Rndr::u32 icon_window;
    Rndr::i32 icon_x, icon_y;
    Rndr::u32 icon_mask;
    Rndr::u32 window_group;
};

constexpr Rndr::u32 k_wm_hint_input = 1 << 0;
constexpr Rndr::u32 k_wm_hint_state = 1 << 1;

constexpr Rndr::u32 k_wm_state_iconic = 3;

}  // namespace

Rndr::LinuxWindow::LinuxWindow(const GenericWindowDesc& desc) : GenericWindow(desc) {}

Opal::Expected<Opal::ScopePtr<Rndr::GenericWindow>, Rndr::ErrorCode> Rndr::LinuxWindow::Create(const GenericWindowDesc& desc)
{
    using ResultType = Opal::Expected<Opal::ScopePtr<GenericWindow>, ErrorCode>;
    if (desc.width <= 0 || desc.height <= 0)
    {
        RNDR_LOG_ERROR("Window width and height must be greater than 0");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (desc.start_minimized && desc.start_maximized)
    {
        RNDR_LOG_ERROR("Window cannot be both minimized and maximized at the same time");
        return ResultType(ErrorCode::InvalidArgument);
    }

    Opal::ScopePtr<GenericWindow> window = Opal::MakeScoped<GenericWindow, LinuxWindow>(Opal::GetDefaultAllocator(), desc);
    const ErrorCode err = static_cast<LinuxWindow*>(window.Get())->Initialize(desc);
    if (err != ErrorCode::Success)
    {
        return ResultType(err);
    }
    return ResultType(std::move(window));
}

Rndr::ErrorCode Rndr::LinuxWindow::Initialize(const GenericWindowDesc& desc)
{
    m_app = LinuxApplication::Get();
    if (m_app == nullptr || m_app->GetConnection() == nullptr)
    {
        RNDR_LOG_ERROR("No X server connection, can't create a window");
        return ErrorCode::PlatformError;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    xcb_screen_t* screen = m_app->GetScreen();
    const LinuxAtoms& atoms = m_app->GetAtoms();

    m_window = xcb_generate_id(connection);
    const u32 event_mask = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS |
                           XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_ENTER_WINDOW |
                           XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    const u32 value_list[] = {event_mask};
    const xcb_void_cookie_t create_cookie = xcb_create_window_checked(
        connection, XCB_COPY_FROM_PARENT, m_window, screen->root, static_cast<i16>(desc.start_x), static_cast<i16>(desc.start_y),
        static_cast<u16>(desc.width), static_cast<u16>(desc.height), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
        XCB_CW_EVENT_MASK, value_list);
    xcb_generic_error_t* error = xcb_request_check(connection, create_cookie);
    if (error != nullptr)
    {
        RNDR_LOG_ERROR("xcb_create_window failed with error code {}", error->error_code);
        free(error);
        m_window = XCB_NONE;
        return ErrorCode::PlatformError;
    }

    // Opt into the window manager's close protocol so a close arrives as a client message
    // instead of the connection being torn down.
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, m_window, atoms.wm_protocols, XCB_ATOM_ATOM, 32, 1, &atoms.wm_delete_window);

    // The title goes to both the legacy and the EWMH property; each carries UTF-8 as-is.
    const Opal::StringUtf8 name = desc.name;
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, m_window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        static_cast<u32>(name.GetSize()), name.GetData());
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, m_window, atoms.net_wm_name, atoms.utf8_string, 8,
                        static_cast<u32>(name.GetSize()), name.GetData());

    m_pos_x = desc.start_x;
    m_pos_y = desc.start_y;
    m_width = desc.width;
    m_height = desc.height;
    ApplyDecorations();
    ApplySizeConstraints();

    // Initial EWMH state has to be a property write - the client message form only works on
    // mapped windows, and this one is not mapped yet.
    xcb_atom_t initial_states[4];
    u32 state_count = 0;
    if (desc.always_on_top)
    {
        initial_states[state_count++] = atoms.net_wm_state_above;
    }
    if (!desc.show_in_taskbar)
    {
        initial_states[state_count++] = atoms.net_wm_state_skip_taskbar;
    }
    if (desc.start_maximized)
    {
        initial_states[state_count++] = atoms.net_wm_state_maximized_horz;
        initial_states[state_count++] = atoms.net_wm_state_maximized_vert;
    }
    if (state_count > 0)
    {
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, m_window, atoms.net_wm_state, XCB_ATOM_ATOM, 32, state_count,
                            initial_states);
    }

    if (desc.start_minimized)
    {
        // The window manager reads the initial state off WM_HINTS when the window is mapped.
        WmHints wm_hints = {};
        wm_hints.flags = k_wm_hint_input | k_wm_hint_state;
        wm_hints.input = 1;
        wm_hints.initial_state = k_wm_state_iconic;
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, m_window, XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 32, 9, &wm_hints);
    }

    m_dpi_scale = m_app->GetDpiScale();
    m_app->ApplyCursorVisibility(m_window);

    if (desc.start_visible)
    {
        xcb_map_window(connection, m_window);
    }
    xcb_flush(connection);

    RNDR_LOG_INFO("Window created successfully!");
    return ErrorCode::Success;
}

Rndr::LinuxWindow::~LinuxWindow()
{
    LinuxWindow::Destroy();
}

Rndr::ErrorCode Rndr::LinuxWindow::RequestClose()
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    const LinuxAtoms& atoms = m_app->GetAtoms();
    m_close_requested = true;
    xcb_client_message_event_t event = {};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = m_window;
    event.type = atoms.wm_protocols;
    event.data.data32[0] = atoms.wm_delete_window;
    event.data.data32[1] = XCB_CURRENT_TIME;
    const xcb_void_cookie_t cookie =
        xcb_send_event_checked(connection, 0, m_window, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char*>(&event));
    xcb_generic_error_t* error = xcb_request_check(connection, cookie);
    if (error != nullptr)
    {
        RNDR_LOG_ERROR("xcb_send_event(WM_DELETE_WINDOW) failed with error code {}", error->error_code);
        free(error);
        m_close_requested = false;
        return ErrorCode::PlatformError;
    }
    xcb_flush(connection);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::LinuxWindow::Reshape(i32 pos_x, i32 pos_y, i32 width, i32 height)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    // X11 has no zero-sized window: xcb_create_window and xcb_configure_window both refuse one with
    // BadValue. Windows allows it (that is what a minimized window reports), so this is a real
    // difference in the contract rather than something to work around here.
    if (width <= 0 || height <= 0)
    {
        RNDR_LOG_ERROR("A window cannot be resized to {}x{}, X11 has no window without a client area", width, height);
        return ErrorCode::InvalidArgument;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    m_pos_x = pos_x;
    m_pos_y = pos_y;
    m_width = width;
    m_height = height;
    // A locked (non-resizable) window has min == max in WM_NORMAL_HINTS, so the lock has to
    // move to the new size before the window manager will allow the resize.
    ApplySizeConstraints();
    const u32 values[] = {static_cast<u32>(pos_x), static_cast<u32>(pos_y), static_cast<u32>(width), static_cast<u32>(height)};
    const xcb_void_cookie_t cookie = xcb_configure_window_checked(
        connection, m_window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
    xcb_generic_error_t* error = xcb_request_check(connection, cookie);
    if (error != nullptr)
    {
        RNDR_LOG_ERROR("xcb_configure_window failed with error code {}", error->error_code);
        free(error);
        return ErrorCode::PlatformError;
    }
    xcb_flush(connection);
    WaitForSize(width, height);
    return ErrorCode::Success;
}

void Rndr::LinuxWindow::WaitForSize(i32 width, i32 height) const
{
    // A configure request is a message to the window manager, so the window still has its old size when
    // this returns, and a caller that reads the size back straight away reads the old one. Windows has no
    // such gap - MoveWindow is applied by the time it returns - so wait for the new size here to keep the
    // one contract on both platforms. The window manager is free to grant something else (or nothing) for
    // a window it constrains, hence the timeout rather than a wait for the exact size.
    constexpr i32 k_max_wait_ms = 500;
    constexpr i32 k_poll_interval_ms = 2;
    for (i32 waited_ms = 0; waited_ms < k_max_wait_ms; waited_ms += k_poll_interval_ms)
    {
        xcb_get_geometry_reply_t* reply =
            xcb_get_geometry_reply(m_app->GetConnection(), xcb_get_geometry(m_app->GetConnection(), m_window), nullptr);
        if (reply == nullptr)
        {
            return;
        }
        const bool has_new_size = reply->width == width && reply->height == height;
        free(reply);
        if (has_new_size)
        {
            return;
        }
        const timespec sleep_time = {.tv_sec = 0, .tv_nsec = k_poll_interval_ms * 1000 * 1000};
        nanosleep(&sleep_time, nullptr);
    }
    RNDR_LOG_WARNING("The window manager did not apply the requested size of {}x{}", width, height);
}

Rndr::ErrorCode Rndr::LinuxWindow::MoveTo(i32 pos_x, i32 pos_y)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    m_pos_x = pos_x;
    m_pos_y = pos_y;
    const u32 values[] = {static_cast<u32>(pos_x), static_cast<u32>(pos_y)};
    const xcb_void_cookie_t cookie =
        xcb_configure_window_checked(connection, m_window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    xcb_generic_error_t* error = xcb_request_check(connection, cookie);
    if (error != nullptr)
    {
        RNDR_LOG_ERROR("xcb_configure_window failed with error code {}", error->error_code);
        free(error);
        return ErrorCode::PlatformError;
    }
    xcb_flush(connection);
    return ErrorCode::Success;
}

void Rndr::LinuxWindow::BringToFront()
{
    if (m_is_closed)
    {
        return;
    }
    if (IsMinimized())
    {
        Restore();
    }
    xcb_connection_t* connection = m_app->GetConnection();
    const LinuxAtoms& atoms = m_app->GetAtoms();
    // Ask the window manager to activate the window; raise it directly as well for window
    // managers that ignore _NET_ACTIVE_WINDOW from regular applications.
    xcb_client_message_event_t event = {};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = m_window;
    event.type = atoms.net_active_window;
    event.data.data32[0] = 1;  // Request comes from a normal application.
    xcb_send_event(connection, 0, m_app->GetScreen()->root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   reinterpret_cast<const char*>(&event));
    const u32 values[] = {XCB_STACK_MODE_ABOVE};
    xcb_configure_window(connection, m_window, XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_flush(connection);
}

void Rndr::LinuxWindow::Destroy()
{
    if (m_window == XCB_NONE || m_app == nullptr || m_app->GetConnection() == nullptr)
    {
        return;
    }
    xcb_destroy_window(m_app->GetConnection(), m_window);
    xcb_flush(m_app->GetConnection());
    m_window = XCB_NONE;
}

void Rndr::LinuxWindow::Minimize()
{
    if (m_is_closed)
    {
        return;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    xcb_client_message_event_t event = {};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = m_window;
    event.type = m_app->GetAtoms().wm_change_state;
    event.data.data32[0] = k_wm_state_iconic;
    xcb_send_event(connection, 0, m_app->GetScreen()->root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   reinterpret_cast<const char*>(&event));
    xcb_flush(connection);
}

void Rndr::LinuxWindow::Maximize()
{
    if (m_is_closed)
    {
        return;
    }
    const LinuxAtoms& atoms = m_app->GetAtoms();
    SendNetWmState(atoms.net_wm_state_maximized_horz, atoms.net_wm_state_maximized_vert, true);
}

void Rndr::LinuxWindow::Restore()
{
    if (m_is_closed)
    {
        return;
    }
    // A minimized X11 window is unmapped; mapping it again is the un-minimize.
    if (IsMinimized())
    {
        xcb_map_window(m_app->GetConnection(), m_window);
        xcb_flush(m_app->GetConnection());
    }
    const LinuxAtoms& atoms = m_app->GetAtoms();
    SendNetWmState(atoms.net_wm_state_maximized_horz, atoms.net_wm_state_maximized_vert, false);
}

void Rndr::LinuxWindow::Enable(bool enable)
{
    // X11 has no per-window input disable, so the event pump drops input events for this
    // window while the flag is off.
    m_is_enabled = enable;
}

void Rndr::LinuxWindow::Show()
{
    if (m_is_closed)
    {
        return;
    }
    xcb_map_window(m_app->GetConnection(), m_window);
    xcb_flush(m_app->GetConnection());
}

void Rndr::LinuxWindow::Hide()
{
    if (m_is_closed)
    {
        return;
    }
    xcb_unmap_window(m_app->GetConnection(), m_window);
    xcb_flush(m_app->GetConnection());
}

void Rndr::LinuxWindow::Focus()
{
    if (m_is_closed)
    {
        return;
    }
    xcb_set_input_focus(m_app->GetConnection(), XCB_INPUT_FOCUS_PARENT, m_window, XCB_CURRENT_TIME);
    xcb_flush(m_app->GetConnection());
}

void Rndr::LinuxWindow::SetMode(GenericWindowMode mode)
{
    if (m_is_closed || m_mode == mode)
    {
        return;
    }
    m_mode = mode;
    SendNetWmState(m_app->GetAtoms().net_wm_state_fullscreen, XCB_ATOM_NONE, mode == GenericWindowMode::BorderlessFullscreen);
}

Rndr::ErrorCode Rndr::LinuxWindow::SetOpacity(f32 opacity)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    const f64 clamped = opacity < 0.0f ? 0.0 : opacity > 1.0f ? 1.0 : static_cast<f64>(opacity);
    const u32 value = static_cast<u32>(clamped * 4294967295.0);  // Full opacity is 0xFFFFFFFF.
    const xcb_void_cookie_t cookie = xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE, m_window,
                                                                 m_app->GetAtoms().net_wm_window_opacity, XCB_ATOM_CARDINAL, 32, 1, &value);
    xcb_generic_error_t* error = xcb_request_check(connection, cookie);
    if (error != nullptr)
    {
        RNDR_LOG_ERROR("Setting _NET_WM_WINDOW_OPACITY failed with error code {}", error->error_code);
        free(error);
        return ErrorCode::PlatformError;
    }
    xcb_flush(connection);
    m_desc.supports_transparency = true;
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::LinuxWindow::SetTitle(const Opal::StringUtf8& title)
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    const LinuxAtoms& atoms = m_app->GetAtoms();
    const xcb_void_cookie_t legacy_cookie = xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE, m_window, XCB_ATOM_WM_NAME,
                                                                       XCB_ATOM_STRING, 8, static_cast<u32>(title.GetSize()),
                                                                       title.GetData());
    const xcb_void_cookie_t ewmh_cookie = xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE, m_window, atoms.net_wm_name,
                                                                      atoms.utf8_string, 8, static_cast<u32>(title.GetSize()),
                                                                      title.GetData());
    xcb_generic_error_t* legacy_error = xcb_request_check(connection, legacy_cookie);
    xcb_generic_error_t* ewmh_error = xcb_request_check(connection, ewmh_cookie);
    const bool failed = legacy_error != nullptr || ewmh_error != nullptr;
    if (failed)
    {
        RNDR_LOG_ERROR("Setting the window title failed");
    }
    free(legacy_error);
    free(ewmh_error);
    if (failed)
    {
        return ErrorCode::PlatformError;
    }
    xcb_flush(connection);
    return ErrorCode::Success;
}

void Rndr::LinuxWindow::SetResizable(bool resizable)
{
    if (m_desc.resizable == resizable)
    {
        return;
    }
    m_desc.resizable = resizable;
    if (m_is_closed || m_window == XCB_NONE)
    {
        return;
    }
    ApplySizeConstraints();
    ApplyDecorations();
}

void Rndr::LinuxWindow::SetTitleBarVisible(bool visible)
{
    if (m_desc.has_title_bar == visible)
    {
        return;
    }
    m_desc.has_title_bar = visible;
    ApplyDecorations();
}

void Rndr::LinuxWindow::SetBorderVisible(bool visible)
{
    if (m_desc.has_border == visible)
    {
        return;
    }
    m_desc.has_border = visible;
    ApplyDecorations();
}

void Rndr::LinuxWindow::SetMinimizeSupported(bool supported)
{
    if (m_desc.supports_minimize == supported)
    {
        return;
    }
    m_desc.supports_minimize = supported;
    ApplyDecorations();
}

void Rndr::LinuxWindow::SetMaximizeSupported(bool supported)
{
    if (m_desc.supports_maximize == supported)
    {
        return;
    }
    m_desc.supports_maximize = supported;
    ApplyDecorations();
}

void Rndr::LinuxWindow::SetCloseSupported(bool supported)
{
    if (m_desc.supports_close == supported)
    {
        return;
    }
    // The event pump vetoes window manager close requests while this is off; the decoration
    // hint just asks the window manager to gray the button out as well.
    m_desc.supports_close = supported;
    ApplyDecorations();
}

void Rndr::LinuxWindow::SetVisibleInTaskbar(bool visible)
{
    if (m_desc.show_in_taskbar == visible)
    {
        return;
    }
    m_desc.show_in_taskbar = visible;
    SendNetWmState(m_app->GetAtoms().net_wm_state_skip_taskbar, XCB_ATOM_NONE, !visible);
}

void Rndr::LinuxWindow::SetAlwaysOnTop(bool always_on_top)
{
    if (m_desc.always_on_top == always_on_top)
    {
        return;
    }
    m_desc.always_on_top = always_on_top;
    SendNetWmState(m_app->GetAtoms().net_wm_state_above, XCB_ATOM_NONE, always_on_top);
}

bool Rndr::LinuxWindow::IsMaximized() const
{
    const LinuxAtoms& atoms = m_app->GetAtoms();
    return HasNetWmState(atoms.net_wm_state_maximized_horz) && HasNetWmState(atoms.net_wm_state_maximized_vert);
}

bool Rndr::LinuxWindow::IsMinimized() const
{
    if (m_window == XCB_NONE)
    {
        return false;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    const LinuxAtoms& atoms = m_app->GetAtoms();
    xcb_get_property_reply_t* reply = xcb_get_property_reply(
        connection, xcb_get_property(connection, 0, m_window, atoms.wm_state, atoms.wm_state, 0, 2), nullptr);
    if (reply == nullptr)
    {
        return false;
    }
    bool is_minimized = false;
    if (xcb_get_property_value_length(reply) >= static_cast<int>(sizeof(u32)))
    {
        const u32 state = *static_cast<const u32*>(xcb_get_property_value(reply));
        is_minimized = state == k_wm_state_iconic;
    }
    free(reply);
    return is_minimized;
}

bool Rndr::LinuxWindow::IsVisible() const
{
    if (m_window == XCB_NONE)
    {
        return false;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    xcb_get_window_attributes_reply_t* reply =
        xcb_get_window_attributes_reply(connection, xcb_get_window_attributes(connection, m_window), nullptr);
    if (reply == nullptr)
    {
        return false;
    }
    const bool is_viewable = reply->map_state == XCB_MAP_STATE_VIEWABLE;
    free(reply);
    // A minimized window is unmapped in X11 but still counts as visible, matching the Windows
    // behaviour of IsWindowVisible for a minimized window.
    return is_viewable || IsMinimized();
}

bool Rndr::LinuxWindow::IsFocused() const
{
    if (m_window == XCB_NONE)
    {
        return false;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    xcb_get_input_focus_reply_t* reply = xcb_get_input_focus_reply(connection, xcb_get_input_focus(connection), nullptr);
    if (reply == nullptr)
    {
        return false;
    }
    const bool is_focused = reply->focus == m_window;
    free(reply);
    return is_focused;
}

bool Rndr::LinuxWindow::IsEnabled() const
{
    return m_is_enabled;
}

bool Rndr::LinuxWindow::IsBorderlessFullscreen() const
{
    return m_mode == GenericWindowMode::BorderlessFullscreen;
}

bool Rndr::LinuxWindow::IsWindowed() const
{
    return m_mode == GenericWindowMode::Windowed;
}

bool Rndr::LinuxWindow::IsResizable() const
{
    return m_desc.resizable;
}

bool Rndr::LinuxWindow::IsMouseHovering() const
{
    if (m_window == XCB_NONE)
    {
        return false;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    xcb_query_pointer_reply_t* reply = xcb_query_pointer_reply(connection, xcb_query_pointer(connection, m_window), nullptr);
    if (reply == nullptr)
    {
        return false;
    }
    const Vector2i size = GetSize();
    const bool is_hovering =
        reply->same_screen != 0 && reply->win_x >= 0 && reply->win_y >= 0 && reply->win_x < size.x && reply->win_y < size.y;
    free(reply);
    return is_hovering;
}

void Rndr::LinuxWindow::EnableHighPrecisionCursorMode(bool enable)
{
    // First cut: mouse deltas come from the regular motion events either way. XInput2 raw
    // motion is the follow-up that would make this flag matter.
    m_high_precision_cursor = enable;
}

bool Rndr::LinuxWindow::IsHighPrecisionCursorModeEnabled() const
{
    return m_high_precision_cursor;
}

Rndr::Vector2i Rndr::LinuxWindow::GetPosition() const
{
    if (m_window == XCB_NONE)
    {
        return Vector2i(m_pos_x, m_pos_y);
    }
    xcb_connection_t* connection = m_app->GetConnection();
    xcb_translate_coordinates_reply_t* reply = xcb_translate_coordinates_reply(
        connection, xcb_translate_coordinates(connection, m_window, m_app->GetScreen()->root, 0, 0), nullptr);
    if (reply == nullptr)
    {
        return Vector2i(m_pos_x, m_pos_y);
    }
    const Vector2i position(reply->dst_x, reply->dst_y);
    free(reply);
    return position;
}

Rndr::Vector2i Rndr::LinuxWindow::GetCursorClientPosition() const
{
    if (m_window == XCB_NONE)
    {
        return {};
    }
    xcb_connection_t* connection = m_app->GetConnection();
    xcb_query_pointer_reply_t* reply = xcb_query_pointer_reply(connection, xcb_query_pointer(connection, m_window), nullptr);
    if (reply == nullptr)
    {
        return {};
    }
    const Vector2i pos(reply->win_x, reply->win_y);
    free(reply);
    return pos;
}

Rndr::Vector2i Rndr::LinuxWindow::GetSize() const
{
    if (m_window == XCB_NONE)
    {
        return Vector2i(m_width, m_height);
    }
    xcb_connection_t* connection = m_app->GetConnection();
    xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(connection, xcb_get_geometry(connection, m_window), nullptr);
    if (reply == nullptr)
    {
        return Vector2i(m_width, m_height);
    }
    const Vector2i size(reply->width, reply->height);
    free(reply);
    return size;
}

Rndr::GenericWindowMode Rndr::LinuxWindow::GetMode() const
{
    return m_mode;
}

Rndr::NativeWindowHandle Rndr::LinuxWindow::GetNativeHandle() const
{
    return reinterpret_cast<NativeWindowHandle>(static_cast<uintptr_t>(m_window));
}

Rndr::NativeDisplayHandle Rndr::LinuxWindow::GetNativeDisplayHandle() const
{
    return reinterpret_cast<NativeDisplayHandle>(m_app != nullptr ? m_app->GetConnection() : nullptr);
}

void Rndr::LinuxWindow::ApplyDecorations()
{
    if (m_window == XCB_NONE)
    {
        return;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    const LinuxAtoms& atoms = m_app->GetAtoms();

    MotifWmHints hints = {};
    hints.flags = k_motif_hints_functions | k_motif_hints_decorations;
    hints.functions = k_motif_func_move;
    if (m_desc.resizable)
    {
        hints.functions |= k_motif_func_resize;
    }
    if (m_desc.supports_minimize)
    {
        hints.functions |= k_motif_func_minimize;
    }
    if (m_desc.supports_maximize)
    {
        hints.functions |= k_motif_func_maximize;
    }
    if (m_desc.supports_close)
    {
        hints.functions |= k_motif_func_close;
    }
    if (m_desc.has_title_bar)
    {
        hints.decorations |= k_motif_decor_title | k_motif_decor_menu;
        // The caption buttons only exist together with the title bar, same as on Windows.
        if (m_desc.supports_minimize)
        {
            hints.decorations |= k_motif_decor_minimize;
        }
        if (m_desc.supports_maximize)
        {
            hints.decorations |= k_motif_decor_maximize;
        }
    }
    // A resizable window always needs its sizing frame, no matter what has_border says.
    if (m_desc.resizable || m_desc.has_border)
    {
        hints.decorations |= k_motif_decor_border | k_motif_decor_resize_handle;
    }
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, m_window, atoms.motif_wm_hints, atoms.motif_wm_hints, 32, 5, &hints);
    xcb_flush(connection);
}

void Rndr::LinuxWindow::ApplySizeConstraints()
{
    if (m_window == XCB_NONE)
    {
        return;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    WmSizeHints hints = {};
    // Without a position hint most window managers ignore the requested start position.
    hints.flags = k_size_hint_p_position;
    hints.x = m_pos_x;
    hints.y = m_pos_y;
    if (!m_desc.resizable)
    {
        hints.flags |= k_size_hint_p_min_size | k_size_hint_p_max_size;
        hints.min_width = m_width;
        hints.min_height = m_height;
        hints.max_width = m_width;
        hints.max_height = m_height;
    }
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, m_window, XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 32, 18, &hints);
    xcb_flush(connection);
}

void Rndr::LinuxWindow::SendNetWmState(xcb_atom_t state_atom, xcb_atom_t second_atom, bool enable)
{
    if (m_is_closed || m_window == XCB_NONE)
    {
        return;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    xcb_client_message_event_t event = {};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = m_window;
    event.type = m_app->GetAtoms().net_wm_state;
    event.data.data32[0] = enable ? 1 : 0;  // _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE
    event.data.data32[1] = state_atom;
    event.data.data32[2] = second_atom;
    event.data.data32[3] = 1;  // Request comes from a normal application.
    xcb_send_event(connection, 0, m_app->GetScreen()->root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   reinterpret_cast<const char*>(&event));
    xcb_flush(connection);
}

bool Rndr::LinuxWindow::HasNetWmState(xcb_atom_t state_atom) const
{
    if (m_window == XCB_NONE || state_atom == XCB_ATOM_NONE)
    {
        return false;
    }
    xcb_connection_t* connection = m_app->GetConnection();
    xcb_get_property_reply_t* reply = xcb_get_property_reply(
        connection, xcb_get_property(connection, 0, m_window, m_app->GetAtoms().net_wm_state, XCB_ATOM_ATOM, 0, 64), nullptr);
    if (reply == nullptr)
    {
        return false;
    }
    const xcb_atom_t* state_atoms = static_cast<const xcb_atom_t*>(xcb_get_property_value(reply));
    const int count = xcb_get_property_value_length(reply) / static_cast<int>(sizeof(xcb_atom_t));
    bool found = false;
    for (int i = 0; i < count; ++i)
    {
        if (state_atoms[i] == state_atom)
        {
            found = true;
            break;
        }
    }
    free(reply);
    return found;
}

#endif  // RNDR_LINUX
