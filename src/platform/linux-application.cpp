#include "rndr/platform/linux-application.hpp"

#if RNDR_LINUX

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <poll.h>

#include <xcb/randr.h>
#include <xcb/xfixes.h>

// xcb/xkb.h is a C header that uses the C++ keyword `explicit` as a struct field name, so the
// keyword has to be renamed for the duration of the include.
#define explicit explicit_field
#include <xcb/xkb.h>
#undef explicit

#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>

#include "rndr/application.hpp"
#include "rndr/log.hpp"
#include "rndr/monitor-info.hpp"
#include "rndr/platform/linux-window.hpp"
#include "rndr/system-message-handler.hpp"

namespace
{

Rndr::LinuxApplication* g_linux_app = nullptr;

Rndr::NativeWindowHandle ToNativeWindowHandle(xcb_window_t window)
{
    return reinterpret_cast<Rndr::NativeWindowHandle>(static_cast<uintptr_t>(window));
}

xcb_window_t ToXcbWindow(Rndr::NativeWindowHandle handle)
{
    return static_cast<xcb_window_t>(reinterpret_cast<uintptr_t>(handle));
}

}  // namespace

Rndr::LinuxApplication* Rndr::LinuxApplication::Get()
{
    return g_linux_app;
}

Rndr::LinuxApplication::LinuxApplication(SystemMessageHandler* message_handler) : PlatformApplication(message_handler)
{
    g_linux_app = this;

    int screen_index = 0;
    m_connection = xcb_connect(nullptr, &screen_index);
    if (xcb_connection_has_error(m_connection) != 0)
    {
        RNDR_LOG_ERROR("Failed to connect to the X server, window creation will not be possible");
        xcb_disconnect(m_connection);
        m_connection = nullptr;
        return;
    }

    const xcb_setup_t* setup = xcb_get_setup(m_connection);
    xcb_screen_iterator_t screen_it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_index && screen_it.rem > 0; ++i)
    {
        xcb_screen_next(&screen_it);
    }
    m_screen = screen_it.data;

    InternAtoms();
    InitializeXkb();
    InitializeRandr();
    InitializeXfixes();
    m_dpi_scale = ReadDpiScale();
}

Rndr::LinuxApplication::~LinuxApplication()
{
    // Windows talk to the X server while being destroyed, so they have to go before the
    // connection does. Drain them the same way ~PlatformApplication would (one at a time,
    // erased from the list before destruction), leaving nothing for the base to tear down.
    while (m_generic_windows.GetSize() > 0)
    {
        auto it = m_generic_windows.begin();
        Opal::ScopePtr<GenericWindow> window = std::move(*it);
        m_generic_windows.Erase(it);
    }

    if (m_xkb_state != nullptr)
    {
        xkb_state_unref(m_xkb_state);
    }
    if (m_xkb_keymap != nullptr)
    {
        xkb_keymap_unref(m_xkb_keymap);
    }
    if (m_xkb_context != nullptr)
    {
        xkb_context_unref(m_xkb_context);
    }
    if (m_connection != nullptr)
    {
        xcb_disconnect(m_connection);
    }
    g_linux_app = nullptr;
}

void Rndr::LinuxApplication::InternAtoms()
{
    auto intern = [this](const char* name) -> xcb_atom_t
    {
        const xcb_intern_atom_cookie_t cookie = xcb_intern_atom(m_connection, 0, static_cast<u16>(strlen(name)), name);
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(m_connection, cookie, nullptr);
        if (reply == nullptr)
        {
            RNDR_LOG_ERROR("Failed to intern atom {}", name);
            return XCB_ATOM_NONE;
        }
        const xcb_atom_t atom = reply->atom;
        free(reply);
        return atom;
    };

    m_atoms.wm_protocols = intern("WM_PROTOCOLS");
    m_atoms.wm_delete_window = intern("WM_DELETE_WINDOW");
    m_atoms.wm_state = intern("WM_STATE");
    m_atoms.wm_change_state = intern("WM_CHANGE_STATE");
    m_atoms.net_wm_name = intern("_NET_WM_NAME");
    m_atoms.utf8_string = intern("UTF8_STRING");
    m_atoms.net_wm_state = intern("_NET_WM_STATE");
    m_atoms.net_wm_state_fullscreen = intern("_NET_WM_STATE_FULLSCREEN");
    m_atoms.net_wm_state_maximized_horz = intern("_NET_WM_STATE_MAXIMIZED_HORZ");
    m_atoms.net_wm_state_maximized_vert = intern("_NET_WM_STATE_MAXIMIZED_VERT");
    m_atoms.net_wm_state_above = intern("_NET_WM_STATE_ABOVE");
    m_atoms.net_wm_state_skip_taskbar = intern("_NET_WM_STATE_SKIP_TASKBAR");
    m_atoms.net_wm_state_hidden = intern("_NET_WM_STATE_HIDDEN");
    m_atoms.net_wm_window_opacity = intern("_NET_WM_WINDOW_OPACITY");
    m_atoms.net_active_window = intern("_NET_ACTIVE_WINDOW");
    m_atoms.motif_wm_hints = intern("_MOTIF_WM_HINTS");
}

void Rndr::LinuxApplication::InitializeXkb()
{
    m_xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (m_xkb_context == nullptr)
    {
        RNDR_LOG_ERROR("Failed to create the xkb context, keyboard input will not work");
        return;
    }

    u16 major_version = 0;
    u16 minor_version = 0;
    u8 base_event = 0;
    u8 base_error = 0;
    if (xkb_x11_setup_xkb_extension(m_connection, XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION,
                                    XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, &major_version, &minor_version, &base_event, &base_error) == 0)
    {
        RNDR_LOG_ERROR("Failed to setup the XKB extension, keyboard input will not work");
        return;
    }
    m_xkb_first_event = base_event;

    m_xkb_device_id = xkb_x11_get_core_keyboard_device_id(m_connection);
    if (m_xkb_device_id == -1)
    {
        RNDR_LOG_ERROR("Failed to get the core keyboard device, keyboard input will not work");
        return;
    }
    m_xkb_keymap = xkb_x11_keymap_new_from_device(m_xkb_context, m_connection, m_xkb_device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (m_xkb_keymap == nullptr)
    {
        RNDR_LOG_ERROR("Failed to compile the keymap, keyboard input will not work");
        return;
    }
    m_xkb_state = xkb_x11_state_new_from_device(m_xkb_keymap, m_connection, m_xkb_device_id);
    if (m_xkb_state == nullptr)
    {
        RNDR_LOG_ERROR("Failed to create the keyboard state, keyboard input will not work");
        return;
    }

    // With detectable auto-repeat the server stops synthesizing a release before every repeated
    // press, so a press while the key is already down is a repeat - same signal the Windows
    // layer gets out of the WM_KEYDOWN repeat bit.
    const xcb_xkb_per_client_flags_cookie_t flags_cookie =
        xcb_xkb_per_client_flags(m_connection, XCB_XKB_ID_USE_CORE_KBD, XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT,
                                 XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT, 0, 0, 0);
    xcb_xkb_per_client_flags_reply_t* flags_reply = xcb_xkb_per_client_flags_reply(m_connection, flags_cookie, nullptr);
    if (flags_reply == nullptr || (flags_reply->value & XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT) == 0)
    {
        RNDR_LOG_WARNING("Detectable auto-repeat is not supported, repeated key presses will not be flagged");
    }
    free(flags_reply);

    // Keep the xkb state (modifiers, layout group) in sync through state notify events instead
    // of feeding it key presses, so changes made outside this application are picked up too.
    xcb_xkb_select_events(m_connection, XCB_XKB_ID_USE_CORE_KBD, XCB_XKB_EVENT_TYPE_STATE_NOTIFY, 0, XCB_XKB_EVENT_TYPE_STATE_NOTIFY, 0, 0,
                          nullptr);
}

void Rndr::LinuxApplication::InitializeRandr()
{
    const xcb_query_extension_reply_t* randr = xcb_get_extension_data(m_connection, &xcb_randr_id);
    if (randr == nullptr || randr->present == 0)
    {
        RNDR_LOG_WARNING("RandR extension not available, monitor info and change events will not work");
        return;
    }
    m_randr_first_event = randr->first_event;
    xcb_randr_select_input(m_connection, m_screen->root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
    xcb_flush(m_connection);
}

void Rndr::LinuxApplication::InitializeXfixes()
{
    const xcb_query_extension_reply_t* xfixes = xcb_get_extension_data(m_connection, &xcb_xfixes_id);
    if (xfixes == nullptr || xfixes->present == 0)
    {
        RNDR_LOG_WARNING("XFixes extension not available, cursor hiding will not work");
        return;
    }
    // The cursor visibility requests need XFixes >= 4, and the version has to be negotiated
    // before any XFixes request is accepted.
    xcb_xfixes_query_version_reply_t* version_reply =
        xcb_xfixes_query_version_reply(m_connection, xcb_xfixes_query_version(m_connection, 4, 0), nullptr);
    m_has_xfixes = version_reply != nullptr && version_reply->major_version >= 4;
    free(version_reply);
    if (!m_has_xfixes)
    {
        RNDR_LOG_WARNING("XFixes is older than version 4, cursor hiding will not work");
    }
}

void Rndr::LinuxApplication::ProcessSystemEvents(u32 timeout_ms)
{
    if (m_connection == nullptr)
    {
        return;
    }

    // Events read from the socket during a reply wait sit in xcb's internal queue where poll(2)
    // cannot see them, so only block on the file descriptor when the queue is confirmed empty.
    xcb_generic_event_t* event = xcb_poll_for_event(m_connection);
    if (event == nullptr && timeout_ms != 0)
    {
        pollfd poll_desc = {};
        poll_desc.fd = xcb_get_file_descriptor(m_connection);
        poll_desc.events = POLLIN;
        const int poll_timeout = timeout_ms == Application::k_infinite_timeout ? -1 : static_cast<int>(timeout_ms);
        poll(&poll_desc, 1, poll_timeout);
        event = xcb_poll_for_event(m_connection);
    }
    while (event != nullptr)
    {
        ProcessEvent(event);
        free(event);
        event = xcb_poll_for_event(m_connection);
    }

    if (m_focused_window.IsValid() && m_focused_window->GetCursorPositionMode() == CursorPositionMode::ResetToCenter)
    {
        const Vector2i size = m_focused_window->GetSize();
        const xcb_window_t window_id = ToXcbWindow(m_focused_window->GetNativeHandle());
        xcb_warp_pointer(m_connection, XCB_NONE, window_id, 0, 0, 0, 0, static_cast<i16>(size.x / 2), static_cast<i16>(size.y / 2));
        // The warp echoes back as a motion event; make sure it neither reports movement nor
        // leaves a stale delta baseline behind.
        m_skip_next_motion = true;
        m_has_last_cursor_pos = false;
    }

    xcb_flush(m_connection);
}

void Rndr::LinuxApplication::ProcessEvent(xcb_generic_event_t* event)
{
    const u8 event_type = event->response_type & 0x7F;

    if (m_xkb_first_event != 0 && event_type == m_xkb_first_event)
    {
        const auto* state_event = reinterpret_cast<xcb_xkb_state_notify_event_t*>(event);
        if (state_event->xkbType == XCB_XKB_STATE_NOTIFY && state_event->deviceID == m_xkb_device_id && m_xkb_state != nullptr)
        {
            xkb_state_update_mask(m_xkb_state, state_event->baseMods, state_event->latchedMods, state_event->lockedMods,
                                  state_event->baseGroup, state_event->latchedGroup, state_event->lockedGroup);
        }
        return;
    }

    if (m_randr_first_event != 0 && event_type == m_randr_first_event + XCB_RANDR_SCREEN_CHANGE_NOTIFY)
    {
        m_message_handler->OnMonitorChange();
        RefreshDpiScale();
        return;
    }

    switch (event_type)
    {
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE:
        {
            // Press and release events share one wire layout.
            const auto* key = reinterpret_cast<xcb_key_press_event_t*>(event);
            Opal::Ref<GenericWindow> window = GetGenericWindowByNativeHandle(ToNativeWindowHandle(key->event));
            if (window == nullptr)
            {
                break;
            }
            auto& window_checked = static_cast<LinuxWindow&>(*window);
            if (!window_checked.m_is_enabled)
            {
                break;
            }
            ProcessKeyEvent(key->detail, window_checked, event_type == XCB_KEY_PRESS);
            break;
        }
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE:
        {
            const auto* button = reinterpret_cast<xcb_button_press_event_t*>(event);
            Opal::Ref<GenericWindow> window = GetGenericWindowByNativeHandle(ToNativeWindowHandle(button->event));
            if (window == nullptr)
            {
                break;
            }
            auto& window_checked = static_cast<LinuxWindow&>(*window);
            if (!window_checked.m_is_enabled)
            {
                break;
            }
            const Vector2i cursor_pos(button->event_x, button->event_y);
            ProcessButtonEvent(button->detail, cursor_pos, button->time, window_checked, event_type == XCB_BUTTON_PRESS);
            break;
        }
        case XCB_MOTION_NOTIFY:
        {
            const auto* motion = reinterpret_cast<xcb_motion_notify_event_t*>(event);
            Opal::Ref<GenericWindow> window = GetGenericWindowByNativeHandle(ToNativeWindowHandle(motion->event));
            if (window == nullptr)
            {
                break;
            }
            auto& window_checked = static_cast<LinuxWindow&>(*window);
            // Deltas are computed in root space so they stay continuous across windows.
            const Vector2i root_pos(motion->root_x, motion->root_y);
            const Vector2i cursor_pos(motion->event_x, motion->event_y);
            f32 delta_x = 0.0f;
            f32 delta_y = 0.0f;
            if (m_has_last_cursor_pos)
            {
                delta_x = static_cast<f32>(root_pos.x - m_last_cursor_pos.x);
                delta_y = static_cast<f32>(root_pos.y - m_last_cursor_pos.y);
            }
            m_last_cursor_pos = root_pos;
            m_has_last_cursor_pos = true;
            if (m_skip_next_motion)
            {
                m_skip_next_motion = false;
                break;
            }
            if (!window_checked.m_is_enabled)
            {
                break;
            }
            m_message_handler->OnMouseMove(window_checked, delta_x, delta_y, cursor_pos);
            break;
        }
        case XCB_ENTER_NOTIFY:
        {
            const auto* enter = reinterpret_cast<xcb_enter_notify_event_t*>(event);
            m_last_cursor_pos = Vector2i(enter->root_x, enter->root_y);
            m_has_last_cursor_pos = true;
            break;
        }
        case XCB_LEAVE_NOTIFY:
        {
            m_has_last_cursor_pos = false;
            break;
        }
        case XCB_FOCUS_IN:
        {
            const auto* focus = reinterpret_cast<xcb_focus_in_event_t*>(event);
            Opal::Ref<GenericWindow> window = GetGenericWindowByNativeHandle(ToNativeWindowHandle(focus->event));
            if (window != nullptr)
            {
                m_focused_window = window.GetPtr();
            }
            break;
        }
        case XCB_FOCUS_OUT:
        {
            const auto* focus = reinterpret_cast<xcb_focus_out_event_t*>(event);
            Opal::Ref<GenericWindow> window = GetGenericWindowByNativeHandle(ToNativeWindowHandle(focus->event));
            if (window != nullptr && m_focused_window.GetPtr() == window.GetPtr())
            {
                m_focused_window = nullptr;
            }
            break;
        }
        case XCB_CONFIGURE_NOTIFY:
        {
            const auto* configure = reinterpret_cast<xcb_configure_notify_event_t*>(event);
            Opal::Ref<GenericWindow> window = GetGenericWindowByNativeHandle(ToNativeWindowHandle(configure->window));
            if (window == nullptr)
            {
                break;
            }
            auto& window_checked = static_cast<LinuxWindow&>(*window);
            window_checked.m_pos_x = configure->x;
            window_checked.m_pos_y = configure->y;
            if (configure->width != window_checked.m_width || configure->height != window_checked.m_height)
            {
                window_checked.m_width = configure->width;
                window_checked.m_height = configure->height;
                m_message_handler->OnWindowSizeChanged(window_checked, configure->width, configure->height);
            }
            break;
        }
        case XCB_CLIENT_MESSAGE:
        {
            const auto* client = reinterpret_cast<xcb_client_message_event_t*>(event);
            Opal::Ref<GenericWindow> window = GetGenericWindowByNativeHandle(ToNativeWindowHandle(client->window));
            if (window == nullptr)
            {
                break;
            }
            auto& window_checked = static_cast<LinuxWindow&>(*window);
            if (client->type == m_atoms.wm_protocols && client->data.data32[0] == m_atoms.wm_delete_window)
            {
                // X11 has no way to gray out the close button, so a close the window does not
                // support is vetoed here instead. RequestClose marks itself so it still works.
                if (!window_checked.IsCloseSupported() && !window_checked.m_close_requested)
                {
                    static bool s_veto_logged = false;
                    if (!s_veto_logged)
                    {
                        s_veto_logged = true;
                        RNDR_LOG_WARNING("Ignoring a window manager close request, the window does not support closing");
                    }
                    break;
                }
                window_checked.m_close_requested = false;
                m_message_handler->OnWindowClose(window_checked);
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

void Rndr::LinuxApplication::ProcessKeyEvent(xcb_keycode_t keycode, LinuxWindow& window, bool is_press)
{
    if (m_xkb_state == nullptr)
    {
        return;
    }
    const u32 keysym = xkb_state_key_get_one_sym(m_xkb_state, keycode);
    InputPrimitive primitive = TranslateKeysym(keysym);
    if (primitive == InputPrimitive::Invalid)
    {
        // A shifted symbol names a different keysym than the key's base level (colon vs
        // semicolon), while the Windows virtual keys the primitives mirror are per-key, not
        // per-symbol. Fall back to the base level of the key before giving up.
        const xkb_layout_index_t layout = xkb_state_key_get_layout(m_xkb_state, keycode);
        const xkb_keysym_t* base_syms = nullptr;
        if (xkb_keymap_key_get_syms_by_level(m_xkb_keymap, keycode, layout, 0, &base_syms) > 0)
        {
            primitive = TranslateKeysym(base_syms[0]);
        }
    }
    if (primitive == InputPrimitive::Invalid)
    {
        RNDR_LOG_ERROR("Keysym {:#X} is not supported!", keysym);
        return;
    }
    if (is_press)
    {
        const bool is_repeated = m_is_key_down[keycode];
        m_is_key_down[keycode] = true;
        m_message_handler->OnButtonDown(window, primitive, is_repeated);
        const uchar32 character = static_cast<uchar32>(xkb_state_key_get_utf32(m_xkb_state, keycode));
        if (character >= 0x20 || character == '\b' || character == '\t' || character == '\r')
        {
            m_message_handler->OnCharacter(window, character, is_repeated);
        }
    }
    else
    {
        m_is_key_down[keycode] = false;
        m_message_handler->OnButtonUp(window, primitive, false);
    }
}

void Rndr::LinuxApplication::ProcessButtonEvent(u8 detail, const Vector2i& cursor_pos, xcb_timestamp_t time, LinuxWindow& window,
                                                bool is_press)
{
    // X11 reports the scroll wheel as button presses: 4/5 vertical, 6/7 horizontal.
    if (detail >= 4 && detail <= 7)
    {
        if (is_press && detail <= 5)
        {
            m_message_handler->OnMouseWheel(window, detail == 4 ? 1.0f : -1.0f, cursor_pos);
        }
        return;
    }

    InputPrimitive primitive = InputPrimitive::Invalid;
    switch (detail)
    {
        case 1:
            primitive = InputPrimitive::Mouse_LeftButton;
            break;
        case 2:
            primitive = InputPrimitive::Mouse_MiddleButton;
            break;
        case 3:
            primitive = InputPrimitive::Mouse_RightButton;
            break;
        case 8:
            primitive = InputPrimitive::Mouse_XButton1;
            break;
        case 9:
            primitive = InputPrimitive::Mouse_XButton2;
            break;
        default:
            return;
    }

    if (!is_press)
    {
        m_message_handler->OnMouseButtonUp(window, primitive, cursor_pos);
        return;
    }

    // X11 has no native double click, so synthesize one out of two presses of the same button
    // close in time and space, the same signal Windows produces with CS_DBLCLKS.
    constexpr u32 k_double_click_time_ms = 500;
    constexpr i32 k_double_click_distance = 4;
    const bool is_double_click = primitive == m_last_click_primitive && (time - m_last_click_time) < k_double_click_time_ms &&
                                 std::abs(cursor_pos.x - m_last_click_pos.x) <= k_double_click_distance &&
                                 std::abs(cursor_pos.y - m_last_click_pos.y) <= k_double_click_distance;
    if (is_double_click)
    {
        // Reset so a triple click reads as a double followed by a single, not two doubles.
        m_last_click_primitive = InputPrimitive::Invalid;
        m_message_handler->OnMouseDoubleClick(window, primitive, cursor_pos);
    }
    else
    {
        m_last_click_primitive = primitive;
        m_last_click_time = time;
        m_last_click_pos = cursor_pos;
        m_message_handler->OnMouseButtonDown(window, primitive, cursor_pos);
    }
}

void Rndr::LinuxApplication::RefreshDpiScale()
{
    const f32 new_dpi_scale = ReadDpiScale();
    if (new_dpi_scale == m_dpi_scale)
    {
        return;
    }
    m_dpi_scale = new_dpi_scale;
    for (const auto& window : m_generic_windows)
    {
        window->SetDpiScale(new_dpi_scale);
        window->on_dpi_change.Execute(new_dpi_scale);
        m_message_handler->OnWindowDpiChanged(*window, new_dpi_scale);
    }
}

Rndr::f32 Rndr::LinuxApplication::ReadDpiScale() const
{
    constexpr f32 k_default_scale = 1.0f;
    xcb_get_property_reply_t* reply = xcb_get_property_reply(
        m_connection, xcb_get_property(m_connection, 0, m_screen->root, XCB_ATOM_RESOURCE_MANAGER, XCB_ATOM_STRING, 0, 16 * 1024),
        nullptr);
    if (reply == nullptr)
    {
        return k_default_scale;
    }
    const char* data = static_cast<const char*>(xcb_get_property_value(reply));
    const int length = xcb_get_property_value_length(reply);
    f32 dpi_scale = k_default_scale;
    for (int i = 0; i + 8 <= length; ++i)
    {
        if ((i == 0 || data[i - 1] == '\n') && memcmp(data + i, "Xft.dpi:", 8) == 0)
        {
            char value_buffer[32] = {};
            int read_pos = i + 8;
            while (read_pos < length && (data[read_pos] == ' ' || data[read_pos] == '\t'))
            {
                ++read_pos;
            }
            int write_pos = 0;
            while (read_pos < length && write_pos < 31 && data[read_pos] != '\n')
            {
                value_buffer[write_pos++] = data[read_pos++];
            }
            const f64 dpi = atof(value_buffer);
            if (dpi > 0.0)
            {
                dpi_scale = static_cast<f32>(dpi / 96.0);
            }
            break;
        }
    }
    free(reply);
    return dpi_scale;
}

void Rndr::LinuxApplication::ShowCursor(bool show)
{
    if (m_connection == nullptr || m_is_cursor_visible == show)
    {
        return;
    }
    m_is_cursor_visible = show;
    if (!m_has_xfixes)
    {
        RNDR_LOG_WARNING("XFixes not available, cursor visibility left unchanged");
        return;
    }
    for (const auto& window : m_generic_windows)
    {
        const xcb_window_t window_id = ToXcbWindow(window->GetNativeHandle());
        if (show)
        {
            xcb_xfixes_show_cursor(m_connection, window_id);
        }
        else
        {
            xcb_xfixes_hide_cursor(m_connection, window_id);
        }
    }
    xcb_flush(m_connection);
}

bool Rndr::LinuxApplication::IsCursorVisible() const
{
    return m_is_cursor_visible;
}

void Rndr::LinuxApplication::ApplyCursorVisibility(xcb_window_t window)
{
    if (m_connection == nullptr || m_is_cursor_visible || !m_has_xfixes)
    {
        return;
    }
    xcb_xfixes_hide_cursor(m_connection, window);
    xcb_flush(m_connection);
}

void Rndr::LinuxApplication::SetCursorPosition(const Vector2i& pos)
{
    if (m_connection == nullptr)
    {
        return;
    }
    xcb_warp_pointer(m_connection, XCB_NONE, m_screen->root, 0, 0, 0, 0, static_cast<i16>(pos.x), static_cast<i16>(pos.y));
    xcb_flush(m_connection);
}

Rndr::Vector2i Rndr::LinuxApplication::GetCursorPosition() const
{
    if (m_connection == nullptr)
    {
        return {};
    }
    xcb_query_pointer_reply_t* reply = xcb_query_pointer_reply(m_connection, xcb_query_pointer(m_connection, m_screen->root), nullptr);
    if (reply == nullptr)
    {
        return {};
    }
    const Vector2i pos(reply->root_x, reply->root_y);
    free(reply);
    return pos;
}

namespace
{

Rndr::i32 GetModeRefreshRate(const xcb_randr_mode_info_t& mode)
{
    if (mode.htotal == 0 || mode.vtotal == 0)
    {
        return 60;
    }
    const Rndr::f64 refresh = static_cast<Rndr::f64>(mode.dot_clock) / (static_cast<Rndr::f64>(mode.htotal) * static_cast<Rndr::f64>(mode.vtotal));
    return static_cast<Rndr::i32>(std::lround(refresh));
}

}  // namespace

Opal::DynamicArray<Rndr::MonitorInfo> Rndr::LinuxApplication::GetMonitors() const
{
    Opal::DynamicArray<MonitorInfo> monitors;
    if (m_connection == nullptr || m_randr_first_event == 0)
    {
        return monitors;
    }
    xcb_randr_get_monitors_reply_t* monitors_reply =
        xcb_randr_get_monitors_reply(m_connection, xcb_randr_get_monitors(m_connection, m_screen->root, 1), nullptr);
    if (monitors_reply == nullptr)
    {
        return monitors;
    }
    xcb_randr_get_screen_resources_current_reply_t* resources = xcb_randr_get_screen_resources_current_reply(
        m_connection, xcb_randr_get_screen_resources_current(m_connection, m_screen->root), nullptr);

    i32 index = 0;
    for (xcb_randr_monitor_info_iterator_t it = xcb_randr_get_monitors_monitors_iterator(monitors_reply); it.rem > 0;
         xcb_randr_monitor_info_next(&it))
    {
        const xcb_randr_monitor_info_t* monitor = it.data;
        MonitorInfo info;
        info.index = index++;
        info.position = Vector2i(monitor->x, monitor->y);
        info.size = Vector2i(monitor->width, monitor->height);
        // X11 exposes the work area per desktop (_NET_WORKAREA), not per monitor, so the full
        // monitor rectangle stands in for it.
        info.work_area_position = info.position;
        info.work_area_size = info.size;
        info.is_primary = monitor->primary != 0;
        info.dpi_scale = m_dpi_scale;
        info.refresh_rate = 60;

        xcb_get_atom_name_reply_t* name_reply =
            xcb_get_atom_name_reply(m_connection, xcb_get_atom_name(m_connection, monitor->name), nullptr);
        if (name_reply != nullptr)
        {
            char name_buffer[128] = {};
            const int name_length = xcb_get_atom_name_name_length(name_reply);
            const int copy_length = name_length < 127 ? name_length : 127;
            memcpy(name_buffer, xcb_get_atom_name_name(name_reply), static_cast<size_t>(copy_length));
            info.name = Opal::StringUtf8(name_buffer);
            free(name_reply);
        }

        // The refresh rate lives on the mode driving the monitor's crtc.
        const xcb_randr_output_t* outputs = xcb_randr_monitor_info_outputs(monitor);
        const int output_count = xcb_randr_monitor_info_outputs_length(monitor);
        if (resources != nullptr && output_count > 0)
        {
            xcb_randr_get_output_info_reply_t* output_info = xcb_randr_get_output_info_reply(
                m_connection, xcb_randr_get_output_info(m_connection, outputs[0], resources->config_timestamp), nullptr);
            if (output_info != nullptr && output_info->crtc != XCB_NONE)
            {
                xcb_randr_get_crtc_info_reply_t* crtc_info = xcb_randr_get_crtc_info_reply(
                    m_connection, xcb_randr_get_crtc_info(m_connection, output_info->crtc, resources->config_timestamp), nullptr);
                if (crtc_info != nullptr)
                {
                    const xcb_randr_mode_info_t* modes = xcb_randr_get_screen_resources_current_modes(resources);
                    const int mode_count = xcb_randr_get_screen_resources_current_modes_length(resources);
                    for (int i = 0; i < mode_count; ++i)
                    {
                        if (modes[i].id == crtc_info->mode)
                        {
                            info.refresh_rate = GetModeRefreshRate(modes[i]);
                            break;
                        }
                    }
                    free(crtc_info);
                }
            }
            free(output_info);
        }

        monitors.PushBack(std::move(info));
    }
    free(resources);
    free(monitors_reply);
    return monitors;
}

Rndr::MonitorInfo Rndr::LinuxApplication::GetPrimaryMonitor() const
{
    Opal::DynamicArray<MonitorInfo> monitors = GetMonitors();
    for (auto& monitor : monitors)
    {
        if (monitor.is_primary)
        {
            return std::move(monitor);
        }
    }
    if (monitors.GetSize() > 0)
    {
        return std::move(monitors[0]);
    }
    return MonitorInfo{};
}

Rndr::MonitorInfo Rndr::LinuxApplication::GetMonitorAtPosition(const Vector2i& pos) const
{
    Opal::DynamicArray<MonitorInfo> monitors = GetMonitors();
    if (monitors.GetSize() == 0)
    {
        return MonitorInfo{};
    }
    // Mirror MONITOR_DEFAULTTONEAREST: the containing monitor, or failing that the closest one.
    i64 best_distance = -1;
    u64 best_index = 0;
    for (u64 i = 0; i < monitors.GetSize(); ++i)
    {
        const MonitorInfo& monitor = monitors[i];
        const i64 dx = pos.x < monitor.position.x                    ? monitor.position.x - pos.x
                       : pos.x >= monitor.position.x + monitor.size.x ? pos.x - (monitor.position.x + monitor.size.x - 1)
                                                                      : 0;
        const i64 dy = pos.y < monitor.position.y                    ? monitor.position.y - pos.y
                       : pos.y >= monitor.position.y + monitor.size.y ? pos.y - (monitor.position.y + monitor.size.y - 1)
                                                                      : 0;
        const i64 distance = dx * dx + dy * dy;
        if (best_distance < 0 || distance < best_distance)
        {
            best_distance = distance;
            best_index = i;
        }
        if (distance == 0)
        {
            break;
        }
    }
    return std::move(monitors[best_index]);
}

Rndr::MonitorInfo Rndr::LinuxApplication::GetMonitorForWindow(const GenericWindow& window) const
{
    const Vector2i position = window.GetPosition();
    const Vector2i size = window.GetSize();
    return GetMonitorAtPosition(Vector2i(position.x + size.x / 2, position.y + size.y / 2));
}

Rndr::InputPrimitive Rndr::LinuxApplication::TranslateKeysym(u32 keysym)
{
    // Letters arrive as the lowercase keysym unless shift is held; the primitives use one value
    // per key, so fold case first.
    if (keysym >= XKB_KEY_a && keysym <= XKB_KEY_z)
    {
        keysym -= XKB_KEY_a - XKB_KEY_A;
    }
    if (keysym >= XKB_KEY_A && keysym <= XKB_KEY_Z)
    {
        return static_cast<InputPrimitive>(static_cast<u16>(InputPrimitive::A) + (keysym - XKB_KEY_A));
    }
    if (keysym >= XKB_KEY_0 && keysym <= XKB_KEY_9)
    {
        return static_cast<InputPrimitive>(static_cast<u16>(InputPrimitive::Digit_0) + (keysym - XKB_KEY_0));
    }
    if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F24)
    {
        return static_cast<InputPrimitive>(static_cast<u16>(InputPrimitive::F1) + (keysym - XKB_KEY_F1));
    }
    if (keysym >= XKB_KEY_KP_0 && keysym <= XKB_KEY_KP_9)
    {
        return static_cast<InputPrimitive>(static_cast<u16>(InputPrimitive::Numpad_0) + (keysym - XKB_KEY_KP_0));
    }
    switch (keysym)
    {
        case XKB_KEY_BackSpace:
            return InputPrimitive::Backspace;
        case XKB_KEY_Tab:
            return InputPrimitive::Tab;
        case XKB_KEY_Clear:
            return InputPrimitive::Clear;
        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
            return InputPrimitive::Return;
        case XKB_KEY_Shift_L:
            return InputPrimitive::LeftShift;
        case XKB_KEY_Shift_R:
            return InputPrimitive::RightShift;
        case XKB_KEY_Control_L:
            return InputPrimitive::LeftCtrl;
        case XKB_KEY_Control_R:
            return InputPrimitive::RightCtrl;
        case XKB_KEY_Alt_L:
            return InputPrimitive::LeftAlt;
        case XKB_KEY_Alt_R:
        case XKB_KEY_ISO_Level3_Shift:  // AltGr
            return InputPrimitive::RightAlt;
        case XKB_KEY_Pause:
            return InputPrimitive::Pause;
        case XKB_KEY_Caps_Lock:
            return InputPrimitive::CapsLock;
        case XKB_KEY_Escape:
            return InputPrimitive::Escape;
        case XKB_KEY_space:
            return InputPrimitive::Space;
        case XKB_KEY_Prior:
        case XKB_KEY_KP_Prior:
            return InputPrimitive::PageUp;
        case XKB_KEY_Next:
        case XKB_KEY_KP_Next:
            return InputPrimitive::PageDown;
        case XKB_KEY_End:
        case XKB_KEY_KP_End:
            return InputPrimitive::End;
        case XKB_KEY_Home:
        case XKB_KEY_KP_Home:
            return InputPrimitive::Home;
        case XKB_KEY_Left:
        case XKB_KEY_KP_Left:
            return InputPrimitive::LeftArrow;
        case XKB_KEY_Up:
        case XKB_KEY_KP_Up:
            return InputPrimitive::UpArrow;
        case XKB_KEY_Right:
        case XKB_KEY_KP_Right:
            return InputPrimitive::RightArrow;
        case XKB_KEY_Down:
        case XKB_KEY_KP_Down:
            return InputPrimitive::DownArrow;
        case XKB_KEY_Insert:
        case XKB_KEY_KP_Insert:
            return InputPrimitive::Insert;
        case XKB_KEY_Delete:
        case XKB_KEY_KP_Delete:
            return InputPrimitive::Delete;
        case XKB_KEY_Super_L:
            return InputPrimitive::LeftLogo;
        case XKB_KEY_Super_R:
            return InputPrimitive::RightLogo;
        case XKB_KEY_KP_Multiply:
            return InputPrimitive::Multiply;
        case XKB_KEY_KP_Add:
            return InputPrimitive::Add;
        case XKB_KEY_KP_Separator:
            return InputPrimitive::Separator;
        case XKB_KEY_KP_Subtract:
            return InputPrimitive::Subtract;
        case XKB_KEY_KP_Decimal:
            return InputPrimitive::Decimal;
        case XKB_KEY_KP_Divide:
            return InputPrimitive::Divide;
        case XKB_KEY_Num_Lock:
            return InputPrimitive::NumLock;
        case XKB_KEY_Scroll_Lock:
            return InputPrimitive::ScrollLock;
        case XKB_KEY_semicolon:
        case XKB_KEY_colon:
            return InputPrimitive::Semicolon;
        case XKB_KEY_equal:
        case XKB_KEY_plus:
            return InputPrimitive::Plus;
        case XKB_KEY_comma:
            return InputPrimitive::Comma;
        case XKB_KEY_minus:
        case XKB_KEY_underscore:
            return InputPrimitive::Minus;
        case XKB_KEY_period:
            return InputPrimitive::Period;
        case XKB_KEY_slash:
        case XKB_KEY_question:
            return InputPrimitive::Slash;
        case XKB_KEY_grave:
        case XKB_KEY_asciitilde:
            return InputPrimitive::Tilde;
        case XKB_KEY_bracketleft:
        case XKB_KEY_braceleft:
            return InputPrimitive::OpenBracket;
        case XKB_KEY_bracketright:
        case XKB_KEY_braceright:
            return InputPrimitive::CloseBracket;
        case XKB_KEY_backslash:
        case XKB_KEY_bar:
            return InputPrimitive::Backslash;
        case XKB_KEY_apostrophe:
        case XKB_KEY_quotedbl:
            return InputPrimitive::Apostrophe;
        case XKB_KEY_less:
        case XKB_KEY_greater:
            return InputPrimitive::Divider;
        default:
            return InputPrimitive::Invalid;
    }
}

#endif  // RNDR_LINUX
