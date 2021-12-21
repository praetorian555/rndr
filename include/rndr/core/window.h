#pragma once

#include <string>

#include "rndr/core/delegate.h"

#include "rndr/render/image.h"

namespace rndr
{

/**
 * Configuration of the window.
 */
struct WindowConfig
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
    Window(const WindowConfig& Options = WindowConfig());

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
     * Get window's color image.
     */
    Image* GetColorImage() { return m_ColorImage; }
    const Image* GetColorImage() const { return m_ColorImage; }

    /**
     * Get window's depth image.
     */
    Image* GetDepthImage() { return m_DepthImage; }
    const Image* GetDepthImage() const { return m_DepthImage; }

    /**
     * Check if window size is 0 along any of the x axis.
     *
     * @return Returns true if we shouldn't render to this window.
     */
    bool IsWindowMinimized() const
    {
        return m_ColorImage->GetConfig().Height == 0 || m_ColorImage->GetConfig().Width == 0;
    }

    /**
     * Copies window's surface buffer to window internal buffer that is displayed on screen.
     */
    void RenderToWindow();

private:
    void Resize(int Width, int Height);

private:
    WindowConfig m_Config;
    NativeWindowHandle m_NativeWindowHandle;

    Image* m_ColorImage = nullptr;
    Image* m_DepthImage = nullptr;

    uint32_t m_CurrentWidth = 0, m_CurrentHeight = 0;
};

/**
 * Represents the state of the keyboard key.
 */
enum class KeyState
{
    Up,
    Down
};

// Check out this for key codes:
// https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
using VirtualKeyCode = uint32_t;

/**
 * Collection of delegates related to window events.
 */
struct WindowDelegates
{
    using ResizeDelegate = MultiDelegate<Window*, int, int>;
    using KeyboardDelegate = MultiDelegate<Window*, KeyState, VirtualKeyCode>;

    /**
     * This delegate is executed when the size of the window's client area is changed.
     */
    static ResizeDelegate OnResize;

    /**
     * This delegate is executed when the keyboard key is pressed or released.
     */
    static KeyboardDelegate OnKeyboardEvent;
};

}  // namespace rndr