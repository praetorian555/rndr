#include <chrono>
#include <iostream>

#include "rndr/core/camera.h"
#include "rndr/core/color.h"
#include "rndr/core/fileutils.h"
#include "rndr/core/log.h"
#include "rndr/core/singletons.h"
#include "rndr/core/threading.h"
#include "rndr/core/transform.h"
#include "rndr/core/window.h"

#include "rndr/render/model.h"
#include "rndr/render/pipeline.h"
#include "rndr/render/rasterizer.h"

#include "rndr/geometry/cube.h"

#include "rndr/profiling/cputracer.h"

#include <Windows.h>

struct ConstantData
{
    rndr::Transform* FromModelToWorld;
    rndr::Camera* Camera;
    rndr::Image* Texture;
};

rndr::Model* CreateModel()
{
    std::shared_ptr<rndr::VertexShader> VertexShader = std::make_shared<rndr::VertexShader>();
    VertexShader->Callback = [](const rndr::PerVertexInfo& Info)
    {
        ConstantData* Constants = (ConstantData*)Info.Constants;
        rndr::CubeVertexData* Data = (rndr::CubeVertexData*)Info.VertexData;

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

    std::shared_ptr<rndr::PixelShader> PixelShader = std::make_shared<rndr::PixelShader>();
    PixelShader->Callback = [](const rndr::PerPixelInfo& Info, real& DepthValue)
    {
        const ConstantData* const Constants = (ConstantData*)Info.Constants;

        const size_t TextureCoordsOffset = offsetof(rndr::CubeVertexData, TextureCoords);
        const rndr::Point2r TexCoord =
            Info.Interpolate<rndr::Point2r, rndr::CubeVertexData>(TextureCoordsOffset);

        const rndr::Vector2r duvdx =
            Info.DerivativeX<rndr::Point2r, rndr::CubeVertexData, rndr::Vector2r>(
                TextureCoordsOffset) *
            Info.NextXMult;

        const rndr::Vector2r duvdy =
            Info.DerivativeY<rndr::Point2r, rndr::CubeVertexData, rndr::Vector2r>(
                TextureCoordsOffset) *
            Info.NextYMult;

        rndr::Color Result = Constants->Texture->Sample(TexCoord, duvdx, duvdy);
        assert(Result.GammaSpace == rndr::GammaSpace::Linear);

        return Result;
    };

    std::shared_ptr<rndr::Pipeline> Pipeline = std::make_shared<rndr::Pipeline>();
    Pipeline->WindingOrder = rndr::WindingOrder::CCW;
    Pipeline->VertexShader = VertexShader;
    Pipeline->PixelShader = PixelShader;
    Pipeline->DepthTest = rndr::DepthTest::LesserThen;

    rndr::Model* Model = new rndr::Model();
    Model->SetPipeline(Pipeline);
    Model->SetVertexData(rndr::Cube::GetVertices());
    Model->SetIndices(rndr::Cube::GetIndices());

    return Model;
}

int main()
{
    rndr::Singletons Singletons;

    rndr::Window Window;
    rndr::Rasterizer Renderer;

    const std::string AssetPath = ASSET_DIR "/SMS_Ranger_Title.bmp";
    rndr::ImageConfig TextureConfig;
    TextureConfig.MagFilter = rndr::ImageFiltering::NearestNeighbor;
    TextureConfig.MinFilter = rndr::ImageFiltering::TrilinearInterpolation;
    std::unique_ptr<rndr::Image> Texture = std::make_unique<rndr::Image>(AssetPath, TextureConfig);

    std::unique_ptr<rndr::Model> Model{CreateModel()};

    const int Width = Window.GetColorImage()->GetConfig().Width;
    const int Height = Window.GetColorImage()->GetConfig().Height;
    const real Near = 0.01;
    const real Far = 1000;
    const real FOV = 90;
    const rndr::Transform FromWorldToCamera = rndr::RotateY(0);
#if 1
    std::shared_ptr<rndr::Camera> Camera =
        std::make_unique<rndr::PerspectiveCamera>(FromWorldToCamera, Width, Height, FOV, Near, Far);
#else
    std::shared_ptr<rndr::Camera> Camera =
        std::make_unique<rndr::OrtographicCamera>(FromWorldToCamera, Width, Height, Near, Far);
#endif

    rndr::WindowDelegates::OnResize.Add([Camera](rndr::Window*, int Width, int Height)
                                        { Camera->UpdateTransforms(Width, Height); });

    real Modifier = 1;
    bool bRotationOn = true;
    real ModelDepth = -60;
    rndr::WindowDelegates::OnKeyboardEvent.Add(
        [&ModelDepth, &bRotationOn, &Modifier](rndr::Window*, rndr::KeyState State,
                                               rndr::VirtualKeyCode KeyCode)
        {
            const real Delta = 10;
            if (State == rndr::KeyState::Down)
            {
                if (KeyCode == 0x51)  // Q
                {
                    ModelDepth -= Delta;
                }
                if (KeyCode == 0x45)  // E
                {
                    ModelDepth += Delta;
                }
                if (KeyCode == 0x20)  // SPACEBAR
                {
                    bRotationOn = !bRotationOn;
                }
                if (KeyCode == 0xBB)  // Plus
                {
                    Modifier += 0.1;
                }
                if (KeyCode == 0xBD)  // Minus
                {
                    Modifier = max(0.1, Modifier - 0.1);
                }
            }
        });

    real TotalTime = 0;
    while (!Window.IsClosed())
    {
        RNDR_CPU_TRACE("Main Loop");

        auto Start = std::chrono::high_resolution_clock().now();

        Window.ProcessEvents();

        if (Window.IsWindowMinimized())
        {
            continue;
        }

        rndr::Image* ColorImage = Window.GetColorImage();
        rndr::Image* DepthImage = Window.GetDepthImage();
        ColorImage->ClearColorBuffer(rndr::Color::White);
        DepthImage->ClearDepthBuffer(rndr::Infinity);

        Model->GetPipeline()->ColorImage = ColorImage;
        Model->GetPipeline()->DepthImage = DepthImage;

        rndr::Transform T = rndr::Translate(rndr::Vector3r(0, 0, ModelDepth)) *
                            rndr::RotateY(0.02 * TotalTime) * rndr::RotateX(0.035 * TotalTime) *
                            rndr ::RotateZ(0.012 * TotalTime) * rndr::Scale(10, 10, 10);

        ConstantData Constants{&T, Camera.get(), Texture.get()};

        Model->SetConstants(Constants);

        Renderer.Draw(Model.get(), 1);
        ColorImage->RenderImage(*Texture, rndr::Point2i{100, 100});

        Window.RenderToWindow();

        auto End = std::chrono::high_resolution_clock().now();
        int Duration = std::chrono::duration_cast<std::chrono::milliseconds>(End - Start).count();

        if (bRotationOn)
        {
            TotalTime += Modifier * Duration;
        }
    }

    return 0;
}
