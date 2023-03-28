#pragma once

#include <string>

#include "math/vector2.h"

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
    static constexpr int kDefaultWindowWidth = 1024;
    static constexpr int kDefaultWindowHeight = 768;

    std::string Name = "Default Window";
};

enum class CursorMode
{
    Normal,
    Hidden,
    Infinite
};

/**
 * Represents a window on the screen and provides a Surface object for rendering.
 */
class Window
{
public:
    using NativeWindowEventDelegate =
        Delegate<bool(NativeWindowHandle, uint32_t, uint64_t, int64_t)>;

    Window() = default;
    ~Window();

    Window(const Window& Other) = delete;
    Window& operator=(const Window& Other) = delete;

    Window(Window&& Other) = delete;
    Window& operator=(Window&& Other) = delete;

    bool Init(int Width = WindowProperties::kDefaultWindowWidth,
              int Height = WindowProperties::kDefaultWindowHeight,
              const WindowProperties& Props = WindowProperties());

    /**
     * Processes events that occurred in the window such as event closing or button press.
     */
    void ProcessEvents() const;

    void Close();

    [[nodiscard]] NativeWindowHandle GetNativeWindowHandle() const;
    [[nodiscard]] int GetWidth() const;
    [[nodiscard]] int GetHeight() const;
    [[nodiscard]] math::Vector2 GetSize() const;

    [[nodiscard]] bool IsWindowMinimized() const;
    [[nodiscard]] bool IsClosed() const;

    bool SetCursorMode(CursorMode Mode);
    CursorMode GetCursorMode() const { return m_CursorMode; }

    void SetNativeWindowEventDelegate(const NativeWindowEventDelegate& Delegate)
    {
        m_OnNativeEvent = Delegate;
    }
    NativeWindowEventDelegate& GetNativeWindowEventDelegate() { return m_OnNativeEvent; }

private:
    void Resize(Window* Window, int Width, int Height);
    void HandleButtonEvent(Window* Window, InputPrimitive Primitive, InputTrigger Trigger);

    WindowProperties m_Props;
    NativeWindowHandle m_NativeWindowHandle;
    NativeWindowEventDelegate m_OnNativeEvent;

    int m_Width = 0;
    int m_Height = 0;
    CursorMode m_CursorMode = CursorMode::Normal;
};

/**
 * Collection of delegates related to window events.
 */
struct WindowDelegates
{
    using ResizeDelegate = MultiDelegate<void(Window*, int, int)>;
    using ButtonDelegate = MultiDelegate<void(Window*, InputPrimitive, InputTrigger)>;
    using MousePositionDelegate = MultiDelegate<void(Window*, int, int)>;
    using RelativeMousePositionDelegate = MultiDelegate<void(Window*, int, int)>;
    using MouseWheelDelegate = MultiDelegate<void(Window*, int)>;

    /**
     * This delegate is executed when the size of the window's client area is changed.
     */
    static ResizeDelegate OnResize;

    /**
     * This delegate is executed when the key on keyboard or mouse is pressed or released.
     */
    static ButtonDelegate OnButtonDelegate;

    /**
     * This delegate is executed when the mouse is moved. The window needs to be in Normal or Hidden cursor mode.
     */
    static MousePositionDelegate OnMousePositionDelegate;

    /**
     * Triggered only if the window is in the Infinite cursor mode.
     */
    static RelativeMousePositionDelegate OnRelativeMousePositionDelegate;

    /**
     * This delegate is executed when the mouse wheel is moved. The value is positive if the wheel
     * is moved away from the user.
     */
    static MouseWheelDelegate OnMouseWheelMovedDelegate;
};

}  // namespace rndr