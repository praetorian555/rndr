#include <chrono>
#include <iostream>

#include "rndr/core/camera.h"
#include "rndr/core/color.h"
#include "rndr/core/transform.h"
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
        {{-50, -50, -10}, {1, 0, 0, 1}}, // 0
        {{ 50, -50, -10}, {0, 1, 0, 1}}, // 1
        {{-50,  50, -10}, {0, 0, 1, 1}}, // 2
        {{ 50,  50, -10}, {1, 1, 0, 1}}, // 3
        {{-50, -50, -100}, {1, 0, 1, 1}}, // 5
        {{ 50, -50, -100}, {0, 1, 1, 1}}, // 4
        {{-50,  50, -100}, {1, 1, 1, 1}}, // 7
        {{ 50,  50, -100}, {0.9, 0.2, 0.3, 1}}, // 6
    };
    // clang-format on

    // clang-format off
    std::vector<int> Indices = {
        0, 1, 3, 0, 3, 2, // front face
        5, 4, 6, 5, 6, 7, // back face
        4, 5, 0, 5, 1, 0, // bottom face
        2, 3, 6, 3, 7, 6, // top face
        4, 0, 6, 0, 2, 6, // left face
        1, 5, 3, 5, 7, 3  // right face

    };
    // clang-format on

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

    const int Width = Window.GetSurface().GetWidth();
    const int Height = Window.GetSurface().GetHeight();
    const int Near = 0;
    const int Far = -1000;
    rndr::Camera* Camera =
        new rndr::OrthographicCamera(rndr::Transform{}, Width, Height, Near, Far);

    rndr::WindowDelegates::OnResize.Add([Camera](rndr::Window*, int Width, int Height)
                                        { Camera->SetFilmSize(Width, Height); });

    real TotalTime = 0;
    while (!Window.IsClosed())
    {
        Window.ProcessEvents();

        if (Window.IsWindowMinimized())
        {
            continue;
        }

        auto Start = std::chrono::steady_clock().now();

        rndr::Surface& Surface = Window.GetSurface();
        Surface.ClearColorBuffer(rndr::Color::Black);
        Surface.ClearDepthBuffer(-std::numeric_limits<real>::infinity());

        rndr::Transform T = rndr::Translate(rndr::Vector3r(0, 0, -60)) *
                            rndr::RotateZ(0.03 * TotalTime) * rndr::RotateY(.05 * TotalTime) *
                            rndr::RotateX(0.02 * TotalTime) *
                            rndr::Translate(rndr::Vector3r(0, 0, 60));

        Model->SetConstants(Camera->FromWorldToNDC() * T);

        Renderer.Draw(Model, 1);

        Window.RenderToWindow();

        auto End = std::chrono::steady_clock().now();
        int Duration = std::chrono::duration_cast<std::chrono::milliseconds>(End - Start).count();
        TotalTime += Duration;

        printf("Duration: %d ms\r", Duration);
    }

    return 0;
}
