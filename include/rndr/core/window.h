#pragma once

#include "rndr/core/base.h"
#include "rndr/core/delegate.h"
#include "rndr/core/forward-def-windows.h"

namespace Rndr
{

/**
 * Represents how the cursor should be displayed and behave.
 */
enum class CursorMode
{
    /**
     * The cursor is visible and his position is represented relative to the window origin. This is
     * the default mode.
     */
    Normal,
    /**
     * The cursor is not visible and his position is represented relative to the window origin.
     */
    Hidden,
    /**
     * The cursor is hidden and his position is represented as a delta relative to the last cursor
     * position. This is useful for first person camera controls.
     */
    Infinite
};

/**
 * Window description.
 */
struct WindowDesc
{
    int width = 1024;
    int height = 768;
    const char* name = "Default Window";
    CursorMode cursor_mode = CursorMode::Normal;
    bool resizable = true;
    bool start_minimized = false;
    bool start_maximized = false;
    bool start_visible = true;
};

/**
 * Private functions used to interface with the OS.
 */
class Window;
namespace WindowPrivate
{
void HandleMouseMove(class Rndr::Window* window, int x, int y);
LRESULT CALLBACK WindowProc(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l);
}  // namespace WindowPrivate

/**
 * Represents a window on the screen.
 */
// TODO(Marko): Add support for multiple monitors.
// TODO(Marko): Add support for fullscreen.
// TODO(Marko): Add support for mouse hover and mouse leave.
class Window
{
public:
    /**
     * Called when the window is resized.
     */
    using ResizeDelegate = MultiDelegate<void(int /*width*/, int /*height*/)>;
    ResizeDelegate on_resize;

    /**
     * Called when the native window event occurs, before the window processes it.
     * @return True if the event was handled and window should not handle it, false otherwise.
     */
    using NativeEventDelegate = Delegate<
        bool(HWND /*window_handle*/, UINT /*msg_code*/, WPARAM /*param_w*/, LPARAM /*param_l*/)>;
    NativeEventDelegate on_native_event;

    /**
     * Creates a new window. If a window failed to create any method will return false.
     * @param desc The description of the window.
     */
    explicit Window(const WindowDesc& desc = WindowDesc{});

    /**
     * Destroys the window.
     */
    ~Window();

    /**
     * Copying is not allowed.
     */
    Window(const Window& other) = delete;
    Window& operator=(const Window& other) = delete;

    /**
     * Move constructor.
     * @param other The other window to move from. It will be invalid window after this.
     */
    Window(Window&& other) noexcept;

    /**
     * Move assignment operator.
     * @param other The other window to move from. It will be invalid window after this.
     * @return Reference to this window.
     */
    Window& operator=(Window&& other) noexcept;

    /**
     * Processes OS events regarding this window.
     */
    void ProcessEvents() const;

    /**
     * Closes the window. This does not destroy the window.
     * @return True if the window was closed successfully.
     */
    void Close() const;

    /**
     * Checks if the window is closed.
     * @return True if the window is closed, false otherwise.
     */
    [[nodiscard]] bool IsClosed() const;

    /**
     * Get the native window handle.
     * @return The native window handle.
     */
    [[nodiscard]] NativeWindowHandle GetNativeWindowHandle() const;

    /**
     * Gets the width of the window.
     * @return The width of the window.
     */
    [[nodiscard]] int GetWidth() const;

    /**
     * Gets the height of the window.
     * @return The height of the window.
     */
    [[nodiscard]] int GetHeight() const;

    /**
     * Gets the size of the window.
     * @return The size of the window as floating-point values.
     */
    [[nodiscard]] math::Vector2 GetSize() const;

    /**
     * Resizes the window.
     * @param width The new width of the window.
     * @param height The new height of the window.
     * @return True if the window was resized successfully.
     * @note Window::GetWidth and Window::GetHeight will be updated only after the next call to
     * Window::ProcessEvents.
     */
    bool Resize(int width, int height) const;

    /**
     * Set if the window is minimized.
     * @param Minimized True if the window should be minimized, false otherwise.
     * @note Window::IsWindowMinimized will be updated only after the next call to
     * Window::ProcessEvents.
     */
    void SetMinimized(bool should_minimize) const;

    /**
     * Checks if the window is minimized.
     * @return True if the window is minimized, false otherwise.
     */
    [[nodiscard]] bool IsWindowMinimized() const;

    /**
     * Set if the window is maximized.
     * @param Maximized True if the window should be maximized, false otherwise.
     * @note Window::IsWindowMaximized will be updated only after the next call to
     * Window::ProcessEvents.
     */
    void SetMaximized(bool should_maximize) const;

    /**
     * Checks if the window is maximized.
     * @return True if the window is maximized, false otherwise.
     */
    [[nodiscard]] bool IsWindowMaximized() const;

    /**
     * Set if the window is visible.
     * @param is_visible True if the window should be visible, false otherwise.
     * @note Window::IsVisible will be updated only after the next call to Window::ProcessEvents.
     */
    void SetVisible(bool is_visible);

    /**
     * Checks if the window is visible.
     * @return True if the window is visible, false otherwise.
     */
    [[nodiscard]] bool IsVisible() const;

    /**
     * Set the cursor mode.
     * @param mode The cursor mode to set.
     * @return True if the cursor mode was set successfully.
     */
    bool SetCursorMode(CursorMode mode);

    /**
     * Gets the cursor mode.
     * @return The cursor mode.
     */
    [[nodiscard]] CursorMode GetCursorMode() const;

private:
    WindowDesc m_desc;
    NativeWindowHandle m_handle = k_invalid_window_handle;
    bool m_is_closed = true;
    bool m_is_minimized = false;
    bool m_is_maximized = false;
    bool m_is_visible = false;

    // Implementation details //////////////////////////////////////////////////////////////////////
    friend void WindowPrivate::HandleMouseMove(Rndr::Window* window, int x, int y);
    friend LRESULT CALLBACK WindowPrivate::WindowProc(HWND window_handle,
                                                      UINT msg_code,
                                                      WPARAM param_w,
                                                      LPARAM param_l);
};

}  // namespace Rndr