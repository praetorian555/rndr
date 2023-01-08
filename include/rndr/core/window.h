#pragma once

#include <string>

#include "rndr/core/base.h"
#include "rndr/core/delegate.h"
#include "rndr/core/inputprimitives.h"

#include "rndr/render/graphicscontext.h"

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
    Window() = default;
    ~Window();

    Window(const Window& Other) = delete;
    Window& operator=(const Window& Other) = delete;

    Window(Window&& Other) = delete;
    Window& operator=(Window&& Other) = delete;

    bool Init(int Width = 1024,
              int Height = 768,
              const WindowProperties& Props = WindowProperties());

    /**
     * Processes events that occured in the window such as event closing or button press.
     */
    void ProcessEvents() const;

    void Close();

    [[nodiscard]] NativeWindowHandle GetNativeWindowHandle() const;
    [[nodiscard]] int GetWidth() const;
    [[nodiscard]] int GetHeight() const;

    [[nodiscard]] bool IsWindowMinimized() const;
    [[nodiscard]] bool IsClosed() const;

    /**
     * If true the cursor will be limited to this window.
     */
    void LockCursor(bool ShouldLock) const;

    /**
     * When activated the cursor will be hidden and you can move inifinitely in any direction.
     * Cursor will also be limited to this window.
     */
    void ActivateInfiniteCursor(bool Activate);

    [[nodiscard]] bool IsInfiniteCursor() const { return m_InifiniteCursor; }

private:
    void Resize(Window* Window, int Width, int Height);
    void ButtonEvent(Window* Window, InputPrimitive Primitive, InputTrigger Trigger);


    WindowProperties m_Props;
    NativeWindowHandle m_NativeWindowHandle;

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