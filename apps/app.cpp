#include <iostream>

#include "rndr/window.h"

int main()
{
    rndr::Window Window;

    while (!Window.IsClosed())
    {
        Window.ProcessEvents();
    }

    return 0;
}
