#include <chrono>
#include <iostream>

#include "rndr/rndr.h"

#include "boxpass.h"

int main()
{
    rndr::RndrApp* RndrApp = rndr::Init();

    rndr::Window* MainWindow = rndr::GRndrApp->GetWindow();
    rndr::ProjectionCameraProperties CameraProps;
    CameraProps.Projection = rndr::ProjectionType::Perspective;
    CameraProps.ScreenWidth = MainWindow->GetWidth();
    CameraProps.ScreenHeight = MainWindow->GetHeight();
    CameraProps.Near = 0.01;
    CameraProps.Far = 100;
    CameraProps.VerticalFOV = 60;
    CameraProps.OrtographicWidth = 50;
    std::shared_ptr<rndr::ProjectionCamera> Camera = std::make_unique<rndr::ProjectionCamera>(math::Transform{}, CameraProps);

    rndr::WindowDelegates::OnResize.Add(
        [Camera, MainWindow](rndr::Window* Window, int Width, int Height)
        {
            if (MainWindow == Window)
            {
                Camera->SetScreenSize(Width, Height);
            }
        });

    rndr::FirstPersonCamera FPCamera(Camera.get(), rndr::Point3r(), 10, 0.1);

    rndr::GraphicsContext* GC = MainWindow->GetGraphicsContext();
    BoxRenderPass BoxPass;
    BoxPass.Init(GC, FPCamera.GetProjectionCamera());

    rndr::GRndrApp->OnTickDelegate.Add(
        [&](real DeltaSeconds)
        {
            FPCamera.Update(DeltaSeconds);
            BoxPass.SetViewerPosition(FPCamera.GetPosition());
            BoxPass.Render(DeltaSeconds);
        });

    rndr::GRndrApp->Run();

    rndr::ShutDown();

    return 0;
}
