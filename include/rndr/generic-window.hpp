#pragma once

#include "opal/container/string.h"

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
    bool resizable = true;
    bool supports_maximize = true;
    bool supports_minimize = true;
    bool supports_transparency = true;
    bool start_minimized = false;
    bool start_maximized = false;
    bool start_visible = true;
    /** If >= 0, center the window on the monitor with this index. Overrides start_x/start_y. */
    int monitor_index = -1;
};

class GenericWindow
{
public:
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
     * Returns true if the window has been marked as closed. This is set automatically when
     * the close request is not vetoed by the on_window_close delegate.
     */
    [[nodiscard]] bool IsClosed() const { return m_is_closed; }

    [[nodiscard]] virtual bool IsMaximized() const = 0;
    [[nodiscard]] virtual bool IsMinimized() const = 0;
    [[nodiscard]] virtual bool IsVisible() const = 0;
    [[nodiscard]] virtual bool IsFocused() const = 0;
    [[nodiscard]] virtual bool IsEnabled() const = 0;
    [[nodiscard]] virtual bool IsBorderless() const = 0;
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

protected:
    GenericWindow(const GenericWindowDesc& desc) : m_desc(desc) {}

    GenericWindowDesc m_desc;
    CursorPositionMode m_cursor_pos_mode = CursorPositionMode::Normal;
    bool m_is_closed = false;

private:
    friend class Application;
    void MarkClosed() { m_is_closed = true; }
};

}  // namespace Rndr