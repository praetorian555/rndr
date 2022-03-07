#include <chrono>
#include <iostream>

#include "rndr/core/camera.h"
#include "rndr/core/color.h"
#include "rndr/core/fileutils.h"
#include "rndr/core/input.h"
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
    rndr::Transform* CameraWorld;
    rndr::Image* Texture;
};

int X, Y;
bool New = false;

rndr::Model* CreateModel()
{
    std::shared_ptr<rndr::VertexShader> VertexShader = std::make_shared<rndr::VertexShader>();
    VertexShader->Callback = [](const rndr::PerVertexInfo& Info, real& W)
    {
        ConstantData* Constants = (ConstantData*)Info.Constants;
        rndr::CubeVertexData* Data = (rndr::CubeVertexData*)Info.VertexData;

        rndr::Point3r WorldSpace = (*Constants->FromModelToWorld)(Data->Position, W);
        WorldSpace = (*Constants->CameraWorld)(WorldSpace, W);
#if 0
        const rndr::Point3r CameraSpace = Constants->Camera->FromWorldToCamera()(WorldSpace, W);
        const rndr::Point3r ScreenSpace = Constants->Camera->FromCameraToScreen()(CameraSpace, W);
        const rndr::Point3r NDCSpace = Constants->Camera->FromScreenToNDC()(ScreenSpace, W);
        return NDCSpace;
#else
        rndr::Point3r NDCSpace = Constants->Camera->FromWorldToNDC()(WorldSpace, W);
        return NDCSpace;
#endif
    };

    std::shared_ptr<rndr::PixelShader> PixelShader = std::make_shared<rndr::PixelShader>();
    PixelShader->Callback = [](const rndr::PerPixelInfo& Info, real& DepthValue)
    {
        const ConstantData* const Constants = (ConstantData*)Info.Constants;

        const size_t TextureCoordsOffset = offsetof(rndr::CubeVertexData, TextureCoords);
        const rndr::Point2r TexCoord =
            Info.Interpolate<rndr::Point2r, rndr::CubeVertexData>(TextureCoordsOffset);

        if (New && Info.Position.X == X && Info.Position.Y == Y)
        {
            RNDR_LOG_INFO("Position=(%d, %d, %.5f), UV=(%.5f, %.5f), Bar(%.5f, %.5f, %.5f)", X, Y,
                          Info.Depth, TexCoord.X, TexCoord.Y, Info.BarCoords[0], Info.BarCoords[1],
                          Info.BarCoords[2]);
            New = false;
        }

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

    const std::string SoldierTexturePath = ASSET_DIR "/SMS_Ranger_Title.bmp";
    const std::string WallTexturePath = ASSET_DIR "/bricked-wall.png";

    rndr::ImageConfig TextureConfig;

    TextureConfig.MagFilter = rndr::ImageFiltering::NearestNeighbor;
    TextureConfig.MinFilter = rndr::ImageFiltering::TrilinearInterpolation;
    std::unique_ptr<rndr::Image> SoldierTexture =
        std::make_unique<rndr::Image>(SoldierTexturePath, TextureConfig);

    TextureConfig.MagFilter = rndr::ImageFiltering::BilinearInterpolation;
    TextureConfig.MinFilter = rndr::ImageFiltering::TrilinearInterpolation;
    std::unique_ptr<rndr::Image> WallTexture =
        std::make_unique<rndr::Image>(WallTexturePath, TextureConfig);

    std::unique_ptr<rndr::Model> Model{CreateModel()};

    const int Width = Window.GetColorImage()->GetConfig().Width;
    const int Height = Window.GetColorImage()->GetConfig().Height;
    const real Near = 0.01;
    const real Far = 1000;
    const real FOV = 90;
    rndr::Transform FromWorldToCamera = rndr::RotateY(0);
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
    bool bRotationOn = false;
    real ModelDepth = -60;
    real RotationAngle = 0;
    real AngleUpDown = 0;
    real AngleLeftRight = 0;

    rndr::InputContext InputContext;
    rndr::InputSystem::Get()->SetContext(&InputContext);

    const rndr::InputAction ZoomAction = "Zoom";
    InputContext.CreateMapping(ZoomAction,
                               [&ModelDepth](rndr::InputBinding Binding, int, int)
                               {
                                   const real Delta = 10;
                                   if (Binding.Primitive == rndr::InputPrimitive::Keyboard_Q)
                                   {
                                       ModelDepth -= Delta;
                                   }
                                   else if (Binding.Primitive == rndr::InputPrimitive::Keyboard_E)
                                   {
                                       ModelDepth += Delta;
                                   }
                               });
    InputContext.AddBinding(ZoomAction, rndr::InputPrimitive::Keyboard_Q,
                            rndr::InputTrigger::Started);
    InputContext.AddBinding(ZoomAction, rndr::InputPrimitive::Keyboard_E,
                            rndr::InputTrigger::Started);

    const rndr::InputAction RotateAction = "Rotate";
    InputContext.CreateMapping(RotateAction,
                               [&RotationAngle](rndr::InputBinding Binding, int, int)
                               {
                                   if (Binding.Primitive == rndr::InputPrimitive::Keyboard_A)
                                   {
                                       RotationAngle -= 1;
                                   }
                                   else if (Binding.Primitive == rndr::InputPrimitive::Keyboard_D)
                                   {
                                       RotationAngle += 1;
                                   }
                               });
    InputContext.AddBinding(RotateAction, rndr::InputPrimitive::Keyboard_A,
                            rndr::InputTrigger::Started);
    InputContext.AddBinding(RotateAction, rndr::InputPrimitive::Keyboard_D,
                            rndr::InputTrigger::Started);

    const rndr::InputAction TurnAroundAction = "TurnAround";
    InputContext.CreateMapping(
        TurnAroundAction,
        [&AngleUpDown, &AngleLeftRight](rndr::InputBinding Binding, int ChangeX, int ChangeY)
        {
            const real Scaler = 0.1;
            if (Binding.Primitive == rndr::InputPrimitive::Mouse_AxisX)
            {
                AngleLeftRight += -Scaler * ChangeX;
            }
            else if (Binding.Primitive == rndr::InputPrimitive::Mouse_AxisY)
            {
                AngleUpDown += Scaler * ChangeY;
            }
        });
    InputContext.AddBinding(TurnAroundAction, rndr::InputPrimitive::Mouse_AxisX,
                            rndr::InputTrigger::Triggered);
    InputContext.AddBinding(TurnAroundAction, rndr::InputPrimitive::Mouse_AxisY,
                            rndr::InputTrigger::Triggered);

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
        ColorImage->ClearColorBuffer(rndr::Color::Black);
        DepthImage->ClearDepthBuffer(rndr::Infinity);

        Model->GetPipeline()->ColorImage = ColorImage;
        Model->GetPipeline()->DepthImage = DepthImage;

        rndr::Transform CameraTransform = rndr::RotateX(AngleUpDown) * rndr::RotateY(AngleLeftRight);
        CameraTransform = CameraTransform.GetInverse();

#if 0
        rndr::Transform R = rndr::RotateY(0.02 * TotalTime) * rndr::RotateX(0.035 * TotalTime) *
                            rndr ::RotateZ(0.012 * TotalTime);
#else
        rndr::Transform R = rndr::RotateY(RotationAngle);
#endif

        rndr::Transform T =
            rndr::Translate(rndr::Vector3r(0, 0, ModelDepth)) * R * rndr::Scale(10, 10, 10);

        ConstantData Constants{&T, Camera.get(), &CameraTransform, WallTexture.get()};

        Model->SetConstants(Constants);

        Renderer.Draw(Model.get(), 1);
        ColorImage->RenderImage(*SoldierTexture, rndr::Point2i{100, 100});

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
