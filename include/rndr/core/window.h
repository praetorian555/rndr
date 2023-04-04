#pragma once

#include "rndr/core/base.h"
#include "rndr/core/delegate.h"

namespace rndr
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
 * Represents a window on the screen.
 */
// TODO(Marko): Add support for multiple monitors.
// TODO(Marko): Add support for fullscreen.
// TODO(Marko): Add support for mouse hover and mouse leave.
class Window
{
public:
    RNDR_NO_CONSTRCUTORS_AND_DESTRUCTOR(Window);

    /**
     * Called when the window is resized.
     */
    using ResizeDelegate = MultiDelegate<void(int /*width*/, int /*height*/)>;
    ResizeDelegate on_resize;

    /**
     * Creates a new window.
     * @param desc The description of the window.
     * @return The created window. If the window could not be created, the returned window will be
     * invalid. Use Window::IsValid to check if the window is valid.
     */
    static Window* Create(const WindowDesc& desc = WindowDesc{});

    /**
     * Destroys the window.
     * @param window The window to destroy.
     */
    static bool Destroy(Window& window);

    /**
     * Checks if the window is valid.
     * @param window The window to check.
     * @return True if the window is valid, false otherwise.
     */
    static bool IsValid(const Window& window);

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
    [[nodiscard]] OpaquePtr GetNativeWindowHandle() const;

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
    void SetVisible(bool is_visible) const;

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
    bool SetCursorMode(CursorMode mode) const;

    /**
     * Gets the cursor mode.
     * @return The cursor mode.
     */
    [[nodiscard]] CursorMode GetCursorMode() const;

private:
    rndr::OpaquePtr m_window_data = nullptr;
};

}  // namespace rndr