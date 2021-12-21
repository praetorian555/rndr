#include <chrono>
#include <iostream>

#include "rndr/core/camera.h"
#include "rndr/core/color.h"
#include "rndr/core/transform.h"
#include "rndr/core/window.h"

#include "rndr/render/model.h"
#include "rndr/render/pipeline.h"
#include "rndr/render/softrenderer.h"

struct VertexData
{
    rndr::Point3r Position;
    rndr::Color Color;
};

struct ConstantData
{
    rndr::Transform* FromModelToWorld;
    rndr::Camera* Camera;
};

rndr::Model* CreateModel()
{
    rndr::VertexShader* VertexShader = new rndr::VertexShader();
    VertexShader->Callback = [](const rndr::PerVertexInfo& Info)
    {
        ConstantData* Constants = (ConstantData*)Info.Constants;
        VertexData* Data = (VertexData*)Info.VertexData;

        const rndr::Point3r WorldSpace = (*Constants->FromModelToWorld)(Data->Position);
#if 0
        const rndr::Point3r CameraSpace = Constants->Camera->FromWorldToCamera()(WorldSpace);
        const rndr::Point3r ScreenSpace = Constants->Camera->FromCameraToScreen()(CameraSpace);
        const rndr::Point3r NDCSpace = Constants->Camera->FromScreenToNDC()(ScreenSpace);
        return NDCSpace;
#else
        return Constants->Camera->FromWorldToNDC()(WorldSpace);
#endif
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
    Pipeline->DepthTest = rndr::DepthTest::LesserThen;
    Pipeline->bApplyGammaCorrection = true;

    // clang-format off
    std::vector<VertexData> Data =
    {
        {{-0.5, -0.5,  0.5}, {1, 0, 0, 1}}, // 0
        {{ 0.5, -0.5,  0.5}, {0, 1, 0, 1}}, // 1
        {{-0.5,  0.5,  0.5}, {0, 0, 1, 1}}, // 2
        {{ 0.5,  0.5,  0.5}, {1, 1, 0, 1}}, // 3
        {{-0.5, -0.5, -0.5}, {1, 0, 1, 1}}, // 5
        {{ 0.5, -0.5, -0.5}, {0, 1, 1, 1}}, // 4
        {{-0.5,  0.5, -0.5}, {1, 1, 1, 1}}, // 7
        {{ 0.5,  0.5, -0.5}, {0.9, 0.2, 0.3, 1}}, // 6
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
    rndr::SoftwareRenderer Renderer;

    rndr::Model* Model = CreateModel();

    const int Width = Window.GetColorImage()->GetConfig().Width;
    const int Height = Window.GetColorImage()->GetConfig().Height;
    const real Near = 0.01;
    const real Far = 100;
    const real FOV = 90;
    const rndr::Transform FromWorldToCamera = rndr::RotateY(0);
#if 1
    rndr::Camera* Camera =
        new rndr::PerspectiveCamera(FromWorldToCamera, Width, Height, FOV, Near, Far);
#else
    rndr::Camera* Camera =
        new rndr::OrthographicCamera(FromWorldToCamera, Width, Height, Near, Far);
#endif

    rndr::WindowDelegates::OnResize.Add([Camera](rndr::Window*, int Width, int Height)
                                        { Camera->UpdateTransforms(Width, Height); });

    real ModelDepth = -60;
    rndr::WindowDelegates::OnKeyboardEvent.Add(
        [&ModelDepth](rndr::Window*, rndr::KeyState State, rndr::VirtualKeyCode KeyCode)
        {
            const real Delta = 10;
            if (State == rndr::KeyState::Down)
            {
                if (KeyCode == 0x51)
                {
                    ModelDepth -= Delta;
                }
                if (KeyCode == 0x45)
                {
                    ModelDepth += Delta;
                }
            }
        });

    real TotalTime = 0;
    while (!Window.IsClosed())
    {
        Window.ProcessEvents();

        if (Window.IsWindowMinimized())
        {
            continue;
        }

        auto Start = std::chrono::steady_clock().now();

        rndr::Image* ColorImage = Window.GetColorImage();
        rndr::Image* DepthImage = Window.GetDepthImage();
        ColorImage->ClearColorBuffer(rndr::Color::Black);
        DepthImage->ClearDepthBuffer(rndr::Infinity);

        Model->GetPipeline()->ColorImage = ColorImage;
        Model->GetPipeline()->DepthImage = DepthImage;

        rndr::Transform T = rndr::Translate(rndr::Vector3r(0, 0, ModelDepth)) *
                            rndr::RotateY(0.02 * TotalTime) * rndr::RotateX(0.035 * TotalTime) *
                            rndr ::RotateZ(0.012 * TotalTime) * rndr::Scale(10, 10, 10);

        ConstantData Constants{&T, Camera};

        Model->SetConstants(Constants);

        Renderer.Draw(Model, 1);

        Window.RenderToWindow();

        auto End = std::chrono::steady_clock().now();
        int Duration = std::chrono::duration_cast<std::chrono::milliseconds>(End - Start).count();
        TotalTime += Duration;

        printf("Duration: %d ms\r", Duration);
    }

    return 0;
}
