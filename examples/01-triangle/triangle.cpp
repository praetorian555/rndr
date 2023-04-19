#include "rndr/rndr.h"

void Run();

/**
 * Basic example on how to use the Rndr library. This example creates a window, initializes a
 * graphics context and a swap chain, sets up a simple vertex and pixel shader, and a pipeline state
 * object. It then draws a triangle to the screen.
 */
int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
}

void Run()
{
    Rndr::Window window({.width = 800, .height = 600, .name = "Triangle"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    assert(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context,
                               {.width = window.GetWidth(), .height = window.GetHeight()});
    assert(swap_chain.IsValid());
    constexpr math::Vector4 k_clear_color{MATH_REALC(0.0),
                                          MATH_REALC(0.0),
                                          MATH_REALC(0.0),
                                          MATH_REALC(1.0)};

    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height)
                          { swap_chain.SetSize(width, height); });

    while (!window.IsClosed())
    {
        window.ProcessEvents();
        graphics_context.Bind(swap_chain);
        graphics_context.ClearColor(k_clear_color);
        graphics_context.Present(swap_chain, true);
    }
}