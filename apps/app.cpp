#include <iostream>

#include "rndr/window.h"

int main()
{
    rndr::Window Window;

    while (!Window.IsClosed())
    {
        Window.ProcessEvents();

        rndr::Surface Surface = Window.GetSurface();

        uint32_t Width = Surface.GetWidth();
        uint32_t Height = Surface.GetHeight();

        Surface.ClearColorBuffer(0x00000000);

        rndr::Vector2i A(400, 400);
        rndr::Vector2i B(600, 600);
        rndr::Vector2i C(500, 650);
        rndr::Vector2i D(600, 200);
        rndr::Vector2i E(500, 150);

        Surface.RenderLine(A, B, 0x00FF0000);
        Surface.RenderLine(A, C, 0x00FF00FF);
        Surface.RenderLine(A, D, 0x0000FF00);
        Surface.RenderLine(A, E, 0x00FFFFFF);

        Surface.RenderBlock({100, 100}, {200, 50}, 0x00FFFF00);

        Window.RenderToWindow();
    }

    return 0;
}
