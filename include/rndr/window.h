#pragma once

#include <string>

namespace rndr
{

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

private:
    WindowOptions m_Options;
    void* m_NativeWindowHandle;
};

}  // namespace rndr