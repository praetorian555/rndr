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

void Run() {}