#include <chrono>
#include <iostream>

#include "rndr/rndr.h"

#include "boxpass.h"
#include "lightpass.h"

int main()
{
    rndr::RndrApp* RndrApp = rndr::Init();

    const std::string SoldierTexturePath = ASSET_DIR "/SMS_Ranger_Title.bmp";
    rndr::ImageConfig TextureConfig;
    TextureConfig.MagFilter = rndr::ImageFiltering::NearestNeighbor;
    TextureConfig.MinFilter = rndr::ImageFiltering::TrilinearInterpolation;
    std::unique_ptr<rndr::Image> SoldierTexture = std::make_unique<rndr::Image>(SoldierTexturePath, TextureConfig);

    rndr::Window* MainWindow = rndr::GRndrApp->GetWindow();
    const int Width = MainWindow->GetWidth();
    const int Height = MainWindow->GetHeight();
    const real Near = 0.01;
    const real Far = 100;
    const real FOVY = 45;
    std::shared_ptr<rndr::Camera> Camera = std::make_unique<rndr::PerspectiveCamera>(rndr::Transform{}, Width, Height, FOVY, Near, Far);

    rndr::WindowDelegates::OnResize.Add(
        [Camera, MainWindow](rndr::Window* Window, int Width, int Height)
        {
            if (MainWindow == Window)
            {
                Camera->UpdateTransforms(Width, Height);
            }
        });
    rndr::InputContext* IC = rndr::GRndrApp->GetInputContext();
    IC->CreateMapping("PrintPixelPosition",
                      [](rndr::InputPrimitive, rndr::InputTrigger, real Value)
                      {
                          const rndr::Point2i Position = rndr::GRndrApp->GetInputSystem()->GetMousePosition();
                          RNDR_LOG_INFO("Mouse Position = (%d, %d)", Position.X, Position.Y);
                      });
    IC->AddBinding("PrintPixelPosition", rndr::InputPrimitive::Mouse_LeftButton, rndr::InputTrigger::ButtonDown);

    rndr::Rasterizer Renderer;

    rndr::FirstPersonCamera FPCamera(Camera.get(), rndr::Point3r(), 10, 1.2);

    BoxRenderPass BoxPass;
    BoxPass.Init(FPCamera.GetProjectionCamera());

    LightRenderPass LightPass;
    LightPass.Init(FPCamera.GetProjectionCamera());

    rndr::GRndrApp->OnTickDelegate.Add(
        [&](real DeltaSeconds)
        {
            FPCamera.Update(DeltaSeconds);

            rndr::Image* ColorImage = MainWindow->GetColorImage();
            rndr::Image* DepthImage = MainWindow->GetDepthImage();
            BoxPass.SetTargetImages(ColorImage, DepthImage);
            BoxPass.SetLightPosition(LightPass.GetLightPosition());
            BoxPass.SetViewerPosition(FPCamera.GetPosition());
            BoxPass.Render(Renderer, DeltaSeconds);

            LightPass.SetTargetImages(ColorImage, DepthImage);
            LightPass.Render(Renderer, DeltaSeconds);

            ColorImage->RenderImage(*SoldierTexture, rndr::Point2i{100, 100});
        });

    rndr::GRndrApp->Run();

    rndr::ShutDown();

    return 0;
}
