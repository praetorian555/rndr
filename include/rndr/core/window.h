#pragma once

#include <string>

#include "rndr/core/delegate.h"
#include "rndr/core/inputprimitives.h"

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
    ~Window() = default;

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
    Image* GetColorImage() { return m_ColorImage.get(); }
    const Image* GetColorImage() const { return m_ColorImage.get(); }

    /**
     * Get window's depth image.
     */
    Image* GetDepthImage() { return m_DepthImage.get(); }
    const Image* GetDepthImage() const { return m_DepthImage.get(); }

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

    int GetWidth() const { return m_ColorImage->GetConfig().Width; }
    int GetHeight() const { return m_ColorImage->GetConfig().Height; }

    /**
     * If true the cursor will be limited to this window.
     */
    void LockCursor(bool ShouldLock);

    /**
     * When activated the cursor will be hidden and you can move inifinitely in any direction.
     * Cursor will also be limited to this window.
     */
    void ActivateInfiniteCursor(bool Activate);

    bool IsInfiniteCursor() const { return m_InifiniteCursor; }

private:
    void Resize(int Width, int Height);

private:
    WindowConfig m_Config;
    NativeWindowHandle m_NativeWindowHandle;

    std::unique_ptr<Image> m_ColorImage = nullptr;
    std::unique_ptr<Image> m_DepthImage = nullptr;

    uint32_t m_CurrentWidth = 0, m_CurrentHeight = 0;

    bool m_InifiniteCursor = false;
};

/**
 * Collection of delegates related to window events.
 */
struct WindowDelegates
{
    using ResizeDelegate = MultiDelegate<Window*, int, int>;
    using ButtonDelegate = MultiDelegate<Window*, InputPrimitive, InputTrigger>;
    using MousePositionDelegate = MultiDelegate<Window*, int, int>;
    using MouseWheelDelegate = MultiDelegate<Window*, int>;

    /**
     * This delegate is executed when the size of the window's client area is changed.
     */
    static ResizeDelegate OnResize;

    /**
     * This delegate is executed when the key on keyboard or mouse is pressed or released.
     */
    static ButtonDelegate OnButtonDelegate;

    /**
     * This delegate is executed when the mouse is moved.
     */
    static MousePositionDelegate OnMousePositionDelegate;

    /**
     * This delegate is executed when the mouse wheel is moved. The value is positive if the wheel
     * is moved away from the user.
     */
    static MouseWheelDelegate OnMouseWheelMovedDelegate;
};

}  // namespace rndr