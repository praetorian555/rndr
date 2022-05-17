#include <chrono>
#include <iostream>

#include "rndr/rndr.h"

#include "boxpass.h"
#include "lightpass.h"

int main()
{
    rndr::RndrApp* RndrApp = rndr::Init();

    rndr::Window* MainWindow = rndr::GRndrApp->GetWindow();
    rndr::ProjectionCameraProperties CameraProps;
    CameraProps.Projection = rndr::ProjectionType::Perspective;
    CameraProps.ScreenWidth = MainWindow->GetWidth();
    CameraProps.ScreenHeight = MainWindow->GetHeight();
    std::shared_ptr<rndr::ProjectionCamera> Camera = std::make_unique<rndr::ProjectionCamera>(math::Transform{}, CameraProps);

    rndr::WindowDelegates::OnResize.Add(
        [Camera, MainWindow](rndr::Window* Window, int Width, int Height)
        {
            if (MainWindow == Window)
            {
                Camera->SetScreenSize(Width, Height);
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

    rndr::FirstPersonCamera FPCamera(Camera.get(), rndr::Point3r(), 10, 1.2);

    rndr::GraphicsContext* GC = MainWindow->GetGraphicsContext();
    BoxRenderPass BoxPass;
    BoxPass.Init(GC, FPCamera.GetProjectionCamera());

    //LightRenderPass LightPass;
    //LightPass.Init(GC, FPCamera.GetProjectionCamera());

    rndr::GRndrApp->OnTickDelegate.Add(
        [&](real DeltaSeconds)
        {
            FPCamera.Update(DeltaSeconds);

            //BoxPass.SetLightPosition(LightPass.GetLightPosition());
            BoxPass.SetViewerPosition(FPCamera.GetPosition());
            BoxPass.Render(DeltaSeconds);

            //LightPass.Render(DeltaSeconds);
        });

    rndr::GRndrApp->Run();

    rndr::ShutDown();

    return 0;
}
