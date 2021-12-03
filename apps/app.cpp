#include <iostream>

#include "rndr/core/color.h"
#include "rndr/core/window.h"

#include "rndr/render/model.h"
#include "rndr/render/pipeline.h"
#include "rndr/render/softrenderer.h"

rndr::Model* CreateModel()
{
    struct VertexData
    {
        rndr::Point3r Position;
        rndr::Color Color;
    };

    rndr::VertexShader* VertexShader = new rndr::VertexShader();
    VertexShader->Callback = [](const rndr::PerVertexInfo& Info)
    {
        VertexData* Data = (VertexData*)Info.VertexData;
        return Data->Position;
    };

    rndr::PixelShader* PixelShader = new rndr::PixelShader();
    PixelShader->Callback = [](const rndr::PerPixelInfo& Info)
    {
        VertexData* Data0 = (VertexData*)Info.VertexData[0];
        VertexData* Data1 = (VertexData*)Info.VertexData[1];
        VertexData* Data2 = (VertexData*)Info.VertexData[2];

        rndr::Color Result;

        // clang-format off
        Result = Data0->Color * Info.Barycentric[0] +
                 Data1->Color * Info.Barycentric[1] +
                 Data2->Color * Info.Barycentric[2];
        // clang-format on

        return Result;
    };

    rndr::Pipeline* Pipeline = new rndr::Pipeline();
    Pipeline->VertexShader = VertexShader;
    Pipeline->PixelShader = PixelShader;
    Pipeline->DepthTest = rndr::DepthTest::GreaterThan;
    Pipeline->bApplyGammaCorrection = true;

    // clang-format off
    std::vector<VertexData> Data =
    {
        {{300, 300, -500}, {1, 0, 0, 0.5}},
        {{500, 300, -1000}, {0, 1, 0, 0.5}},
        {{300, 500, -750}, {0, 0, 1, 0.5}},
        {{500, 500, -2000}, {1, 1, 0, 0.5}}
    };
    // clang-format on

    std::vector<int> Indices = {0, 1, 3, 0, 3, 2};

    rndr::Model* Model = new rndr::Model();
    Model->SetPipeline(Pipeline);
    Model->SetVertexData(Data);
    Model->SetIndices(Indices);

    return Model;
}

int main()
{
    rndr::Window Window;
    rndr::SoftwareRenderer Renderer(&Window.GetSurface());

    rndr::Model* Model = CreateModel();

    while (!Window.IsClosed())
    {
        Window.ProcessEvents();

        if (Window.IsWindowMinimized())
        {
            continue;
        }

        rndr::Surface Surface = Window.GetSurface();
        Surface.ClearColorBuffer(rndr::Color::Black);
        Surface.ClearDepthBuffer(-std::numeric_limits<real>::infinity());

        Renderer.Draw(Model);

        Window.RenderToWindow();
    }

    return 0;
}
