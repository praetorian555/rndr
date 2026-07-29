#pragma once

#include "opal/container/string.h"
#include "opal/delegate.h"

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
};

class GenericWindow
{
public:
    using DpiChangeDelegate = Opal::MultiDelegate<void(f32 /*new_dpi_scale*/)>;
    /** Fired when the OS reports a DPI change for this window. */
    DpiChangeDelegate on_dpi_change;

    virtual ~GenericWindow() = default;

    /**
     * Requests closing of the window. Should trigger Application::on_window_close as if the user pressed x in the UI.
     */
    virtual void RequestClose() = 0;

    virtual void Reshape(i32 pos_x, i32 pos_y, i32 width, i32 height) = 0;
    virtual void MoveTo(i32 pos_x, i32 pos_y) = 0;
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
    virtual void SetOpacity(f32 opacity) = 0;
    virtual void SetTitle(const Opal::StringUtf8& title) = 0;

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

    [[nodiscard]] virtual Vector2i GetPosition() const = 0;
    [[nodiscard]] virtual Vector2i GetSize() const = 0;
    [[nodiscard]] virtual GenericWindowMode GetMode() const = 0;
    [[nodiscard]] virtual NativeWindowHandle GetNativeHandle() const = 0;

    /**
     * Returns the current DPI scale factor for this window (1.0 == 96 DPI). Updated by the platform
     * whenever the OS reports a DPI change.
     */
    [[nodiscard]] f32 GetDpiScale() const { return m_dpi_scale; }

protected:
    GenericWindow(const GenericWindowDesc& desc) : m_desc(desc) {}

    GenericWindowDesc m_desc;
    CursorPositionMode m_cursor_pos_mode = CursorPositionMode::Normal;
    bool m_is_closed = false;
    f32 m_dpi_scale = 1.0f;

private:
    friend class Application;
    friend class PlatformApplication;
    friend class WindowsApplication;
    void MarkClosed() { m_is_closed = true; }
    void SetDpiScale(f32 dpi_scale) { m_dpi_scale = dpi_scale; }
};

}  // namespace Rndr