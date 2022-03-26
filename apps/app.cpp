#include <chrono>
#include <iostream>

#include "rndr/rndr.h"

#include "boxpass.h"

int main()
{
    rndr::RndrApp* RndrApp = rndr::Init();

    const std::string SoldierTexturePath = ASSET_DIR "/SMS_Ranger_Title.bmp";
    rndr::ImageConfig TextureConfig;
    TextureConfig.MagFilter = rndr::ImageFiltering::NearestNeighbor;
    TextureConfig.MinFilter = rndr::ImageFiltering::TrilinearInterpolation;
    std::unique_ptr<rndr::Image> SoldierTexture =
        std::make_unique<rndr::Image>(SoldierTexturePath, TextureConfig);

    rndr::Window* MainWindow = rndr::GRndrApp->GetWindow();
    const int Width = MainWindow->GetWidth();
    const int Height = MainWindow->GetHeight();
    const real Near = 0.01;
    const real Far = 100;
    const real FOVY = 45;
    std::shared_ptr<rndr::Camera> Camera = std::make_unique<rndr::PerspectiveCamera>(
        rndr::Transform{}, Width, Height, FOVY, Near, Far);

    rndr::WindowDelegates::OnResize.Add([Camera](rndr::Window*, int Width, int Height)
                                        { Camera->UpdateTransforms(Width, Height); });

    rndr::Rasterizer Renderer;

    rndr::FirstPersonCamera FPCamera(Camera.get());

    BoxRenderPass BoxPass;
    rndr::Image* ColorImage = MainWindow->GetColorImage();
    rndr::Image* DepthImage = MainWindow->GetDepthImage();
    BoxPass.Init(ColorImage, DepthImage, FPCamera.GetProjectionCamera());

    rndr::GRndrApp->OnTickDelegate.Add(
        [&](real DeltaSeconds)
        {
            FPCamera.Update(DeltaSeconds);

            BoxPass.Render(Renderer, DeltaSeconds);

            ColorImage->RenderImage(*SoldierTexture, rndr::Point2i{100, 100});
        });

    rndr::GRndrApp->Run();

    rndr::ShutDown();

    return 0;
}
