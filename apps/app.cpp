#include <iostream>

#include "rndr/color.h"
#include "rndr/render/pipeline.h"
#include "rndr/render/softrenderer.h"
#include "rndr/window.h"

int main()
{
    rndr::Window Window;
    rndr::SoftwareRenderer Renderer(&Window.GetSurface());

    rndr::PixelShader Shader;
    Shader.Callback = [](const rndr::PerPixelInfo& Info)
    {
        rndr::Color Result;
        Result = rndr::Color::Red * Info.Barycentric[0] + rndr::Color::Green * Info.Barycentric[1] +
                 rndr::Color::Blue * Info.Barycentric[2];
        return Result;
    };

    rndr::Pipeline Pipeline;
    Pipeline.bApplyGammaCorrection = true;
    Pipeline.PixelShader = &Shader;

    Renderer.SetPipeline(&Pipeline);

    while (!Window.IsClosed())
    {
        Window.ProcessEvents();

        if (Window.IsWindowMinimized())
        {
            continue;
        }

        rndr::Surface Surface = Window.GetSurface();

        uint32_t Width = Surface.GetWidth();
        uint32_t Height = Surface.GetHeight();

        Surface.ClearColorBuffer(rndr::Color::Black);

        rndr::Point2i A(400, 400);
        rndr::Point2i B(600, 600);
        rndr::Point2i C(500, 650);
        rndr::Point2i D(600, 200);
        rndr::Point2i E(500, 150);

        Surface.RenderLine(A, B, rndr::Color::Red);
        Surface.RenderLine(A, C, rndr::Color::Green);
        Surface.RenderLine(A, D, rndr::Color::Blue);
        Surface.RenderLine(A, E, rndr::Color(0, 1, 1, 1));

        Surface.RenderBlock({100, 100}, {200, 50}, rndr::Color(0, 1, 1, 1));

        std::vector<rndr::Point3r> Positions = {
            {100, 300, 0}, {300, 300, 0}, {300, 500, 0}, {100, 500, 0}};
        std::vector<int> Indices = {0, 1, 2, 0, 2, 3};

        Renderer.DrawTriangles(Positions, Indices);

        Window.RenderToWindow();
    }

    return 0;
}
