#include <chrono>
#include <iostream>

#include <Windows.h>

#include "rndr/rndr.h"

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
    rndr::RndrApp* RndrApp = rndr::Init();

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

    rndr::Window* MainWindow = rndr::GRndrApp->GetWindow();
    const int Width = MainWindow->GetWidth();
    const int Height = MainWindow->GetHeight();
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

    rndr::InputContext* InputContext = rndr::GRndrApp->GetInputContext();

    const rndr::InputAction ZoomAction = "Zoom";
    auto ZoomHandler = [&ModelDepth](rndr::InputPrimitive Primitive, rndr::InputTrigger, real)
    {
        const real Delta = 1;
        if (Primitive == rndr::InputPrimitive::Keyboard_Q)
        {
            ModelDepth -= Delta;
        }
        else if (Primitive == rndr::InputPrimitive::Keyboard_E)
        {
            ModelDepth += Delta;
        };
    };
    InputContext->CreateMapping(ZoomAction, ZoomHandler);
    InputContext->AddKeyBinding(ZoomAction, rndr::InputPrimitive::Keyboard_Q,
                                rndr::InputTrigger::ButtonDown);
    InputContext->AddKeyBinding(ZoomAction, rndr::InputPrimitive::Keyboard_E,
                                rndr::InputTrigger::ButtonDown);

    const rndr::InputAction RotateAction = "Rotate";
    auto RotateHandler = [&RotationAngle](rndr::InputPrimitive Primitive, rndr::InputTrigger, real)
    {
        if (Primitive == rndr::InputPrimitive::Keyboard_A)
        {
            RotationAngle -= 1;
        }
        else if (Primitive == rndr::InputPrimitive::Keyboard_D)
        {
            RotationAngle += 1;
        }
    };
    InputContext->CreateMapping(RotateAction, RotateHandler);
    InputContext->AddKeyBinding(RotateAction, rndr::InputPrimitive::Keyboard_A,
                                rndr::InputTrigger::ButtonDown);
    InputContext->AddKeyBinding(RotateAction, rndr::InputPrimitive::Keyboard_D,
                                rndr::InputTrigger::ButtonDown);

    const rndr::InputAction TurnAroundAction = "TurnAround";
    auto TurnAroundHandler = [&AngleUpDown, &AngleLeftRight](rndr::InputPrimitive Primitive,
                                                             rndr::InputTrigger, real Value)
    {
        const real Scaler = 100;
        if (Primitive == rndr::InputPrimitive::Mouse_AxisX)
        {
            AngleLeftRight += Scaler * Value;
        }
        else if (Primitive == rndr::InputPrimitive::Mouse_AxisY)
        {
            AngleUpDown += Scaler * Value;
        }
    };
    InputContext->CreateMapping(TurnAroundAction, TurnAroundHandler);
    InputContext->AddAxisBinding(TurnAroundAction, rndr::InputPrimitive::Mouse_AxisX,
                                 rndr::InputTrigger::AxisChanged, -1);
    InputContext->AddAxisBinding(TurnAroundAction, rndr::InputPrimitive::Mouse_AxisY,
                                 rndr::InputTrigger::AxisChanged);

    rndr::Rasterizer Renderer;

    rndr::GRndrApp->OnTickDelegate.Add(
        [&](real DeltaSeconds)
        {
            rndr::Window* MainWindow = rndr::GRndrApp->GetWindow();

            rndr::Image* ColorImage = MainWindow->GetColorImage();
            rndr::Image* DepthImage = MainWindow->GetDepthImage();

            Model->GetPipeline()->ColorImage = ColorImage;
            Model->GetPipeline()->DepthImage = DepthImage;

            rndr::Transform CameraTransform =
                rndr::RotateX(AngleUpDown) * rndr::RotateY(AngleLeftRight);
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
        });

    rndr::GRndrApp->Run();

    rndr::ShutDown();

    return 0;
}
