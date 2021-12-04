#pragma once

#include <string>

#include "delegate.h"
#include "rndr.h"
#include "surface.h"

namespace rndr
{

/**
 * Configuration of the window.
 */
struct WindowOptions
{
    std::string Name = "Default Window";

    int Width = 1024;
    int Height = 768;
};

/**
 * Represents a window on the screen and provides a Surface object for rendering.
 */
class Window
{
public:
    Window(const WindowOptions& Options = WindowOptions());

    /**
     * Processes events that occured in the window such as event closing or button press.
     */
    void ProcessEvents();

    /**
     * Check if window is closed.
     */
    bool IsClosed() const;

    /**
     * Close the window.
     */
    void Close();

    /**
     * Get OS window handle.
     */
    NativeWindowHandle GetNativeWindowHandle() const { return m_NativeWindowHandle; }

    /**
     * Get window surface to which user can render.
     */
    Surface& GetSurface() { return *m_Surface; }

    /**
     * Check if window size is 0 along any of the x axis.
     *
     * @return Returns true if we shouldn't render to this window.
     */
    bool IsWindowMinimized() const
    {
        return m_Surface->GetHeight() == 0 || m_Surface->GetWidth() == 0;
    }

    /**
     * Copies window's surface buffer to window internal buffer that is displayed on screen.
     */
    void RenderToWindow();

private:
    void Resize(int Width, int Height);

private:
    WindowOptions m_Options;
    NativeWindowHandle m_NativeWindowHandle;

    Surface* m_Surface = nullptr;

    uint32_t m_CurrentWidth = 0, m_CurrentHeight = 0;
};

/**
 * Collection of delegates related to window events.
 */
struct WindowDelegates
{
    using ResizeDelegate = MultiDelegate<Window*, int, int>;

    /**
     * This delegate is executed when the size of the window's client area is changed.
     */
    static ResizeDelegate OnResize;
};

}  // namespace rndr