#pragma once

#include "opal/container/string.h"
#include "opal/delegate.h"

#include "rndr/error-codes.hpp"
#include "rndr/math.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

enum class GenericWindowMode : u8
{
    Windowed,
    BorderlessFullscreen,

    Count
};

/**
 * Which way up the screen shows the application. Only a phone or a tablet turns; a desktop window has no
 * orientation, and there it is only recorded.
 */
enum class ScreenOrientation : u8
{
    /** Whatever the system does for an application that asks nothing: follows the device within the user's rotation lock. */
    Any,
    /** Landscape, either way up, turning over when the device is turned over. */
    Landscape,
    /** Portrait, either way up, turning over when the device is turned over. */
    Portrait
};

/**
 * How far in from each edge of a window the system covers or cuts into it: the status and navigation bars, a camera
 * cutout, rounded corners the cutout accounts for. Pixels, in the window's own coordinates - the space GetSize and the
 * cursor are in. Content the user has to see or touch goes inside them; a background can run under them.
 */
struct SafeInsets
{
    i32 left = 0;
    i32 top = 0;
    i32 right = 0;
    i32 bottom = 0;

    bool operator==(const SafeInsets& other) const = default;
};

/**
 * Represents how the window should modify cursor's position.
 */
enum class CursorPositionMode : u8
{
    /**
     * The cursor is moved by the user and will stay there until moved again. Default behaviour.
     */
    Normal,
    /**
     * The cursor is moved by the user, but it's reset to the center of the window every frame (this reset will not trigger mouse position
     * update). Useful for FPS games.
     */
    ResetToCenter
};

/**
 * The shape the OS draws the cursor in while it is over a window's client area. Over the frame and the
 * title bar the OS keeps choosing the shape itself, so the sizing arrows still show on a resizable border.
 */
enum class CursorShape : u8
{
    /** The standard arrow. Default. */
    Arrow,
    /** A text caret, over editable text. */
    IBeam,
    /** A pointing hand, over a link or another clickable target. */
    Hand,
    /** A crosshair, for precise picking. */
    Crosshair,
    /** A two-headed horizontal arrow, over something resized left and right (a vertical splitter). */
    ResizeHorizontal,
    /** A two-headed vertical arrow, over something resized up and down (a horizontal splitter). */
    ResizeVertical,
    /** A two-headed arrow from top-left to bottom-right. */
    ResizeDiagonalDown,
    /** A two-headed arrow from bottom-left to top-right. */
    ResizeDiagonalUp,
    /** A four-headed arrow, over something moved in any direction. */
    ResizeAll,
    /** A slashed circle, over something that refuses the current action (a drop target that rejects a drag). */
    NotAllowed,
    /** A busy indicator: the application cannot take input. */
    Wait,
    /** An arrow with a busy indicator: the application is working but still takes input. */
    Progress,
    /** An arrow with a question mark. */
    Help,

    Count
};

struct GenericWindowDesc
{
    int width = 1024;
    int height = 768;
    int start_x = 0;
    int start_y = 0;
    const char* name = "Default Window";
    /** Window can be resized by dragging its frame. Implies the presence of a sizing frame. */
    bool resizable = true;
    /** Maximize caption button is enabled. The button is still drawn, only grayed out, when false. */
    bool supports_maximize = true;
    /** Minimize caption button is enabled. The button is still drawn, only grayed out, when false. */
    bool supports_minimize = true;
    /**
     * Close caption button is enabled. When false the button is grayed out and the OS close shortcut
     * (Alt+F4 on Windows) is disabled. RequestClose still works, so the app can always close the window itself.
     */
    bool supports_close = true;
    /**
     * Window can be made translucent through SetOpacity. SetOpacity enables this on demand, so it only needs to be
     * set here if transparency has to be available from the very first presented frame.
     * @note On Windows this creates a layered window, which can slow down or break hardware-accelerated presentation
     * on some drivers. Off by default for that reason.
     */
    bool supports_transparency = false;
    /** Window has a title bar showing the window name and the caption buttons. */
    bool has_title_bar = true;
    /**
     * Window has a frame drawn around the client area. Ignored when resizable is true, since a resizable window
     * always needs a sizing frame. Set both this and has_title_bar to false for a fully undecorated window.
     */
    bool has_border = true;
    /** Window gets its own button in the OS task bar. */
    bool show_in_taskbar = true;
    /** Window stays above all other non-topmost windows, even when it loses focus. */
    bool always_on_top = false;
    bool start_minimized = false;
    bool start_maximized = false;
    bool start_visible = true;
    /** If >= 0, center the window on the monitor with this index. Overrides start_x/start_y. */
    int monitor_index = -1;
    /**
     * Which way up the screen shows the application; see GenericWindow::SetOrientation. On Android it is asked for
     * before the first native window arrives, so that window already has the orientation. Any leaves the activity
     * as its manifest declares it.
     */
    ScreenOrientation orientation = ScreenOrientation::Any;
    /** See GenericWindow::SetPreferredRefreshRate. 0 for no preference. */
    f32 preferred_refresh_rate = 0.0f;
};

class GenericWindow
{
public:
    using DpiChangeDelegate = Opal::MultiDelegate<void(f32 /*new_dpi_scale*/)>;
    /**
     * Fired when the OS reports a DPI change for this window. On Windows the window then resizes to the
     * rect the OS suggests, so it keeps its size in logical units, and the resize is reported as usual.
     */
    DpiChangeDelegate on_dpi_change;

    virtual ~GenericWindow() = default;

    /**
     * Requests closing of the window. Should trigger Application::on_window_close as if the user pressed x in the UI.
     * @return ErrorCode::WindowAlreadyClosed if the window is closed, ErrorCode::PlatformError if the OS refuses.
     */
    virtual ErrorCode RequestClose() = 0;

    virtual ErrorCode Reshape(i32 pos_x, i32 pos_y, i32 width, i32 height) = 0;
    virtual ErrorCode MoveTo(i32 pos_x, i32 pos_y) = 0;
    virtual void BringToFront() = 0;
    virtual void Destroy() = 0;
    virtual void Minimize() = 0;
    virtual void Maximize() = 0;
    virtual void Restore() = 0;
    virtual void Enable(bool enable) = 0;
    virtual void Show() = 0;
    virtual void Hide() = 0;
    virtual void Focus() = 0;
    virtual void SetMode(GenericWindowMode mode) = 0;
    virtual ErrorCode SetOpacity(f32 opacity) = 0;
    virtual ErrorCode SetTitle(const Opal::StringUtf8& title) = 0;

    /**
     * Decoration and behaviour toggles. All of them can be used at any point after the window is created and
     * mirror the matching fields of GenericWindowDesc. Changes to the frame are applied immediately, which means
     * the client area keeps its size but the outer window size changes.
     */
    virtual void SetResizable(bool resizable) = 0;
    /** Show or hide the title bar. Hiding it also hides the caption buttons. */
    virtual void SetTitleBarVisible(bool visible) = 0;
    /** Show or hide the frame around the client area. Ignored while the window is resizable. */
    virtual void SetBorderVisible(bool visible) = 0;
    virtual void SetMinimizeSupported(bool supported) = 0;
    virtual void SetMaximizeSupported(bool supported) = 0;
    /** Enable or disable the close button and the OS close shortcut. RequestClose is unaffected. */
    virtual void SetCloseSupported(bool supported) = 0;
    /** Add or remove the task bar button of this window. */
    virtual void SetVisibleInTaskbar(bool visible) = 0;
    virtual void SetAlwaysOnTop(bool always_on_top) = 0;

    [[nodiscard]] bool HasTitleBar() const { return m_desc.has_title_bar; }
    [[nodiscard]] bool HasBorder() const { return m_desc.has_border; }
    [[nodiscard]] bool IsMinimizeSupported() const { return m_desc.supports_minimize; }
    [[nodiscard]] bool IsMaximizeSupported() const { return m_desc.supports_maximize; }
    [[nodiscard]] bool IsCloseSupported() const { return m_desc.supports_close; }
    [[nodiscard]] bool IsVisibleInTaskbar() const { return m_desc.show_in_taskbar; }
    [[nodiscard]] bool IsAlwaysOnTop() const { return m_desc.always_on_top; }

    /** Returns the description this window was created with, updated by the setters above. */
    [[nodiscard]] const GenericWindowDesc& GetDesc() const { return m_desc; }

    /**
     * Returns true if the window has been marked as closed. This is set automatically when
     * the close request is not vetoed by the on_window_close delegate.
     */
    [[nodiscard]] bool IsClosed() const { return m_is_closed; }

    [[nodiscard]] virtual bool IsMaximized() const = 0;
    [[nodiscard]] virtual bool IsMinimized() const = 0;
    [[nodiscard]] virtual bool IsVisible() const = 0;
    [[nodiscard]] virtual bool IsFocused() const = 0;
    [[nodiscard]] virtual bool IsEnabled() const = 0;
    /** True while the window is in GenericWindowMode::BorderlessFullscreen. See HasBorder for the frame of a windowed window. */
    [[nodiscard]] virtual bool IsBorderlessFullscreen() const = 0;
    [[nodiscard]] virtual bool IsResizable() const = 0;
    [[nodiscard]] virtual bool IsWindowed() const = 0;
    [[nodiscard]] virtual bool IsMouseHovering() const = 0;

    /**
     * Control if the OS provides more frequent and fine-grained cursor movement updates for this window.
     * @param enable If the mode should be enabled or not.
     * @note On Windows this will trigger the generation of WM_INPUT system events.
     */
    virtual void EnableHighPrecisionCursorMode(bool enable) = 0;

    /**
     * Check if high-precision cursor mode is enabled for this window.
     */
    [[nodiscard]] virtual bool IsHighPrecisionCursorModeEnabled() const = 0;

    void SetCursorPositionMode(CursorPositionMode mode) { m_cursor_pos_mode = mode; }
    [[nodiscard]] CursorPositionMode GetCursorPositionMode() const { return m_cursor_pos_mode; }

    /**
     * Set the shape of the cursor while it is over this window's client area. Takes effect at once when the cursor
     * is already there, and whenever it comes back. Whether the cursor shows at all is Application::ShowCursor.
     * @param shape The shape to draw. CursorShape::Count is ignored.
     * @note Applied on Windows. On Linux the shape is recorded, and GetCursorShape reports it, but the cursor
     * keeps its shape.
     */
    virtual void SetCursorShape(CursorShape shape)
    {
        if (shape != CursorShape::Count)
        {
            m_cursor_shape = shape;
        }
    }

    /** Returns the shape set by SetCursorShape; CursorShape::Arrow until one is set. */
    [[nodiscard]] CursorShape GetCursorShape() const { return m_cursor_shape; }

    /**
     * Ask for the screen to show the application this way up. On Android the activity turns to match, and the window
     * is resized and the swap chain recreated as for any rotation. A desktop window has no orientation, so there the
     * value is only recorded.
     * @return ErrorCode::PlatformError when Android refuses the request, which leaves the orientation as it was.
     */
    virtual ErrorCode SetOrientation(ScreenOrientation orientation)
    {
        m_orientation = orientation;
        return ErrorCode::Success;
    }

    /** The orientation last asked for, through SetOrientation or GenericWindowDesc::orientation. */
    [[nodiscard]] ScreenOrientation GetOrientation() const { return m_orientation; }

    /**
     * The insets the system takes from this window. All zero on the desktop, where the window's client area is the
     * application's. On Android they are read when the window arrives and again when it is resized or the
     * configuration changes, before the resize is reported, so reading them is free and a resize handler already
     * sees the new ones.
     */
    [[nodiscard]] SafeInsets GetSafeInsets() const { return m_safe_insets; }

    /**
     * Ask for the display to refresh at this rate while the window is shown, in hertz; 0 for no preference. A hint:
     * on Android the system weighs it against the panel's modes, the user's settings, battery saver and heat, and
     * only switches when the switch is seamless. The rate it settles on is MonitorInfo::refresh_rate, and a change
     * is reported through Application::on_monitor_change. A desktop display's rate is not an application's to set,
     * so there it is only recorded.
     * @return ErrorCode::InvalidArgument for a negative or non-finite rate, which is not recorded;
     *         ErrorCode::PlatformError when Android refuses it, which is recorded and asked for again with the next
     *         native window.
     */
    virtual ErrorCode SetPreferredRefreshRate(f32 rate)
    {
        if (!(rate >= 0.0f) || rate > 1.0e6f)
        {
            return ErrorCode::InvalidArgument;
        }
        m_preferred_refresh_rate = rate;
        return ErrorCode::Success;
    }

    /** The rate last asked for, through SetPreferredRefreshRate or GenericWindowDesc::preferred_refresh_rate. */
    [[nodiscard]] f32 GetPreferredRefreshRate() const { return m_preferred_refresh_rate; }

    [[nodiscard]] virtual Vector2i GetPosition() const = 0;
    [[nodiscard]] virtual Vector2i GetSize() const = 0;

    /**
     * Returns the current cursor position in this window's client-space coordinates, with the origin at
     * the top-left corner of the client area. This is the same space the mouse events report their
     * cursor position in. The result can be negative or larger than GetSize when the cursor is outside
     * the window. Returns (0, 0) when the position cannot be queried.
     */
    [[nodiscard]] virtual Vector2i GetCursorClientPosition() const = 0;
    [[nodiscard]] virtual GenericWindowMode GetMode() const = 0;
    [[nodiscard]] virtual NativeWindowHandle GetNativeHandle() const = 0;

    /**
     * Returns the platform object that owns this window's native handle: the HINSTANCE of the
     * module on Windows, the xcb_connection_t* on Linux. Rendering surface creation needs it
     * alongside GetNativeHandle.
     */
    [[nodiscard]] virtual NativeDisplayHandle GetNativeDisplayHandle() const = 0;

    /**
     * Returns the current DPI scale factor for this window (1.0 == 96 DPI). Updated by the platform
     * whenever the OS reports a DPI change.
     */
    [[nodiscard]] f32 GetDpiScale() const { return m_dpi_scale; }

protected:
    GenericWindow(const GenericWindowDesc& desc)
        : m_desc(desc), m_orientation(desc.orientation), m_preferred_refresh_rate(desc.preferred_refresh_rate > 0.0f ? desc.preferred_refresh_rate : 0.0f)
    {
    }

    GenericWindowDesc m_desc;
    CursorPositionMode m_cursor_pos_mode = CursorPositionMode::Normal;
    CursorShape m_cursor_shape = CursorShape::Arrow;
    ScreenOrientation m_orientation = ScreenOrientation::Any;
    SafeInsets m_safe_insets;
    f32 m_preferred_refresh_rate = 0.0f;
    bool m_is_closed = false;
    f32 m_dpi_scale = 1.0f;

private:
    friend class Application;
    friend class PlatformApplication;
    friend class WindowsApplication;
    friend class LinuxApplication;
    friend class AndroidApplication;
    void MarkClosed() { m_is_closed = true; }
    void SetDpiScale(f32 dpi_scale) { m_dpi_scale = dpi_scale; }
};

}  // namespace Rndr