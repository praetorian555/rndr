#pragma once

#include <string>

#include "rndr/core/base.h"
#include "rndr/core/delegate.h"
#include "rndr/core/inputprimitives.h"

#if defined RNDR_RASTER
#include "rndr/raster/rastergraphicscontext.h"
#include "rndr/raster/rasterimage.h"
#else
#error "Image implementation is missing!"
#endif

namespace rndr
{

/**
 * Configuration of the window.
 */
struct WindowProperties
{
    std::string Name = "Default Window";
};

/**
 * Represents a window on the screen and provides a Surface object for rendering.
 */
class Window
{
public:
    Window(int Width = 1024, int Height = 768, const WindowProperties& Props = WindowProperties());
    ~Window() = default;

    /**
     * Processes events that occured in the window such as event closing or button press.
     */
    void ProcessEvents();

    bool IsClosed() const;
    void Close();

    NativeWindowHandle GetNativeWindowHandle() const;
    int GetWidth() const;
    int GetHeight() const;
    bool IsWindowMinimized() const;
    GraphicsContext* GetGraphicsContext() const;

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
    void Resize(Window* Window, int Width, int Height);
    void ButtonEvent(Window* Window, InputPrimitive Primitive, InputTrigger Trigger);

private:
    WindowProperties m_Props;
    NativeWindowHandle m_NativeWindowHandle;

    std::unique_ptr<GraphicsContext> m_GraphicsContext;
    int m_Width = 0, m_Height = 0;

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