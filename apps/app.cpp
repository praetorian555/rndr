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

        uint32_t Color = 0x0000FF00;
        for (uint32_t Y = 0; Y < Height; Y++)
        {
            for (uint32_t X = 0; X < Width; X++)
            {
                Surface.SetPixel(X, Y, Color);
            }
        }

        Window.RenderToWindow();
    }

    return 0;
}
