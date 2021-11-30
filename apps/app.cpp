#include <iostream>

#include "rndr/core/color.h"
#include "rndr/core/window.h"

#include "rndr/render/pipeline.h"
#include "rndr/render/softrenderer.h"

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

        std::vector<rndr::Point3r> Positions = {
            {300, 300, 0}, {500, 300, 0}, {300, 500, 0}, {500, 500, 0}};
        std::vector<int> Indices = {0, 1, 2, 1, 2, 3};

        Renderer.DrawTriangles(Positions, Indices);

        Window.RenderToWindow();
    }

    return 0;
}
