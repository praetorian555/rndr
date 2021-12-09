#include <chrono>
#include <iostream>

#include "rndr/core/color.h"
#include "rndr/core/transform.h"
#include "rndr/core/window.h"
#include "rndr/core/camera.h"

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
        rndr::Transform* T = (rndr::Transform*)Info.Constants;

        VertexData* Data = (VertexData*)Info.VertexData;
        return (*T)(Data->Position);
    };

    rndr::PixelShader* PixelShader = new rndr::PixelShader();
    PixelShader->Callback = [](const rndr::PerPixelInfo& Info, real& DepthValue)
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
    Pipeline->WindingOrder = rndr::WindingOrder::CCW;
    Pipeline->VertexShader = VertexShader;
    Pipeline->PixelShader = PixelShader;
    Pipeline->DepthTest = rndr::DepthTest::GreaterThan;
    Pipeline->bApplyGammaCorrection = true;

    // clang-format off
    std::vector<VertexData> Data =
    {
        {{-2, -2, -5}, {1, 0, 0, 0.5}},
        {{ 2, -2, -5}, {0, 1, 0, 0.5}},
        {{-2,  2, -5}, {0, 0, 1, 0.5}},
        {{ 2,  2, -5}, {1, 1, 0, 0.5}}
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

    int FilmWidth = 5;
    int FilmHeight = 5;
    int Near = 0;
    int Far = -10;
    rndr::Camera* Camera = new rndr::OrthographicCamera(rndr::Transform{}, FilmWidth, FilmHeight, Near, Far);

    real TotalTime = 0;
    while (!Window.IsClosed())
    {
        Window.ProcessEvents();

        if (Window.IsWindowMinimized())
        {
            continue;
        }

        auto Start = std::chrono::steady_clock().now();

        rndr::Surface Surface = Window.GetSurface();
        Surface.ClearColorBuffer(rndr::Color::Black);
        Surface.ClearDepthBuffer(-std::numeric_limits<real>::infinity());

        Model->SetConstants(Camera->FromWorldToNDC());

        Renderer.Draw(Model, 100);

        Window.RenderToWindow();

        auto End = std::chrono::steady_clock().now();
        int Duration = std::chrono::duration_cast<std::chrono::milliseconds>(End - Start).count();
        TotalTime += Duration;

        printf("Duration: %d ms\r", Duration);
    }

    return 0;
}
