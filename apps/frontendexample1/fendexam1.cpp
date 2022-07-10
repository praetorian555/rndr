#include "rndr/core/rndrapp.h"
#include "rndr/core/setup.h"

#include "rndr/ui/uisystem.h"

class rndr::GraphicsContext;
class rndr::Window;

static void AppLoop(float DeltaSeconds);

rndr::ui::ImageId LogoImage;

int main()
{
    rndr::RndrApp* App = rndr::Init();
    App->OnTickDelegate.Add(AppLoop);

    rndr::GraphicsContext* Context = App->GetWindow()->GetGraphicsContext();
    rndr::ui::UIProperties UIProps;
    rndr::ui::Init(Context, UIProps);

    LogoImage = rndr::ui::AddImage("C:/dev/rndr/apps/frontendexample1/logo.png");
    assert(LogoImage != rndr::ui::kInvalidImageId);

    App->Run();

    rndr::ui::ShutDown();
    rndr::ShutDown();
}

void AppLoop(float DeltaSeconds)
{
    rndr::ui::StartFrame();

    math::Vector4 BackgroundColor{0.5, 0.5, 0.5, 1};
    rndr::ui::SetColor(BackgroundColor);

    rndr::ui::ImageBoxProperties IBProps;
    IBProps.BottomLeft = math::Point2{0, 0};
    IBProps.ImageId = LogoImage;
    IBProps.Scale = 0.5f;
    rndr::ui::DrawImageBox(IBProps);

    rndr::ui::EndFrame();
}
