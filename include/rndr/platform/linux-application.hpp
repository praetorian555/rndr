#pragma once

#include "rndr/definitions.hpp"

#if RNDR_LINUX

#include <xcb/xcb.h>

#include "opal/container/dynamic-array.h"

#include "rndr/generic-window.hpp"
#include "rndr/input-primitives.hpp"
#include "rndr/platform-application.hpp"

struct xkb_context;
struct xkb_keymap;
struct xkb_state;

namespace Rndr
{

/**
 * The atoms the platform layer talks to the window manager with, interned once at startup.
 * Windows borrow them through GetAtoms, the same way they borrow the connection.
 */
struct LinuxAtoms
{
    xcb_atom_t wm_protocols = XCB_ATOM_NONE;
    xcb_atom_t wm_delete_window = XCB_ATOM_NONE;
    xcb_atom_t wm_state = XCB_ATOM_NONE;
    xcb_atom_t wm_change_state = XCB_ATOM_NONE;
    xcb_atom_t net_wm_name = XCB_ATOM_NONE;
    xcb_atom_t utf8_string = XCB_ATOM_NONE;
    xcb_atom_t net_wm_state = XCB_ATOM_NONE;
    xcb_atom_t net_wm_state_fullscreen = XCB_ATOM_NONE;
    xcb_atom_t net_wm_state_maximized_horz = XCB_ATOM_NONE;
    xcb_atom_t net_wm_state_maximized_vert = XCB_ATOM_NONE;
    xcb_atom_t net_wm_state_above = XCB_ATOM_NONE;
    xcb_atom_t net_wm_state_skip_taskbar = XCB_ATOM_NONE;
    xcb_atom_t net_wm_state_hidden = XCB_ATOM_NONE;
    xcb_atom_t net_wm_window_opacity = XCB_ATOM_NONE;
    xcb_atom_t net_active_window = XCB_ATOM_NONE;
    xcb_atom_t motif_wm_hints = XCB_ATOM_NONE;
};

class LinuxApplication : public PlatformApplication
{
public:
    LinuxApplication(struct SystemMessageHandler* message_handler);
    ~LinuxApplication() override;

    void ProcessSystemEvents(u32 timeout_ms) override;

    void ShowCursor(bool show) override;
    [[nodiscard]] bool IsCursorVisible() const override;
    void SetCursorPosition(const Vector2i& pos) override;
    [[nodiscard]] Vector2i GetCursorPosition() const override;

    [[nodiscard]] Opal::DynamicArray<MonitorInfo> GetMonitors() const override;
    [[nodiscard]] MonitorInfo GetPrimaryMonitor() const override;
    [[nodiscard]] MonitorInfo GetMonitorAtPosition(const Vector2i& pos) const override;
    [[nodiscard]] MonitorInfo GetMonitorForWindow(const GenericWindow& window) const override;

    /** The one live instance, or null. LinuxWindow uses it to borrow the connection. */
    static LinuxApplication* Get();

    [[nodiscard]] xcb_connection_t* GetConnection() const { return m_connection; }
    [[nodiscard]] xcb_screen_t* GetScreen() const { return m_screen; }
    [[nodiscard]] const LinuxAtoms& GetAtoms() const { return m_atoms; }
    [[nodiscard]] f32 GetDpiScale() const { return m_dpi_scale; }

    /** Applies the application-wide cursor visibility to one window. Called on window creation. */
    void ApplyCursorVisibility(xcb_window_t window);

private:
    void InternAtoms();
    void InitializeXkb();
    void InitializeRandr();
    void InitializeXfixes();

    void ProcessEvent(xcb_generic_event_t* event);
    void ProcessKeyEvent(xcb_keycode_t keycode, class LinuxWindow& window, bool is_press);
    void ProcessButtonEvent(u8 detail, const Vector2i& cursor_pos, xcb_timestamp_t time, class LinuxWindow& window, bool is_press);
    void RefreshDpiScale();

    /** Parses Xft.dpi out of the root RESOURCE_MANAGER property. Falls back to 96 DPI. */
    [[nodiscard]] f32 ReadDpiScale() const;

    /** Maps an xkb keysym to the matching InputPrimitive, InputPrimitive::Invalid when there is none. */
    [[nodiscard]] static InputPrimitive TranslateKeysym(u32 keysym);

    xcb_connection_t* m_connection = nullptr;
    xcb_screen_t* m_screen = nullptr;
    LinuxAtoms m_atoms;

    xkb_context* m_xkb_context = nullptr;
    xkb_keymap* m_xkb_keymap = nullptr;
    xkb_state* m_xkb_state = nullptr;
    i32 m_xkb_device_id = -1;
    u8 m_xkb_first_event = 0;

    u8 m_randr_first_event = 0;
    bool m_has_xfixes = false;
    bool m_is_cursor_visible = true;
    f32 m_dpi_scale = 1.0f;

    /** Keycode-indexed down state, used to flag auto-repeated key presses. */
    bool m_is_key_down[256] = {};

    /** Root-space cursor position of the last motion event, the baseline for mouse deltas. */
    Vector2i m_last_cursor_pos;
    bool m_has_last_cursor_pos = false;
    /** Set after a ResetToCenter warp so its echoed motion event is not reported as movement. */
    bool m_skip_next_motion = false;

    /** Double clicks are synthesized from two presses of the same button close in time and space. */
    xcb_timestamp_t m_last_click_time = 0;
    InputPrimitive m_last_click_primitive = InputPrimitive::Invalid;
    Vector2i m_last_click_pos;
};

}  // namespace Rndr

#endif  // RNDR_LINUX
