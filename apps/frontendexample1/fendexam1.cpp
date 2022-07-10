#include "rndr/core/rndrapp.h"
#include "rndr/core/setup.h"

#include "rndr/ui/uisystem.h"

class rndr::GraphicsContext;
class rndr::Window;

static void AppLoop(float DeltaSeconds);

static void LoadAssets();

constexpr int kDefaultFontSize = 18;

rndr::ui::ImageId ArrowUpImage;
rndr::ui::ImageId ArrowDownImage;
rndr::ui::ImageId CalendarImage;
rndr::ui::ImageId CloseMenuImage;
rndr::ui::ImageId MenuImage;
rndr::ui::ImageId PlanningImage;
rndr::ui::ImageId RemindersImage;
rndr::ui::ImageId TodoImage;
rndr::ui::ImageId LogoImage;
rndr::ui::ImageId ClientAudiophileImage;
rndr::ui::ImageId ClientDataBizImage;
rndr::ui::ImageId ClientMakerImage;
rndr::ui::ImageId ClientMeetImage;

rndr::ui::FontHandle EpilogueMediumFont;
rndr::ui::FontHandle EpilogueBoldFont;

int main()
{
    rndr::RndrApp* App = rndr::Init();
    App->OnTickDelegate.Add(AppLoop);

    rndr::GraphicsContext* Context = App->GetWindow()->GetGraphicsContext();
    rndr::ui::UIProperties UIProps;
    rndr::ui::Init(Context, UIProps);

    LoadAssets();

    App->Run();

    rndr::ui::ShutDown();
    rndr::ShutDown();
}

void AppLoop(float DeltaSeconds)
{
    rndr::ui::StartFrame();

    math::Vector4 BackgroundColor{1, 1, 1, 1};
    rndr::ui::SetColor(BackgroundColor);

    rndr::ui::ImageBoxProperties IBProps;
    IBProps.BottomLeft = math::Point2{0, 0};
    IBProps.ImageId = LogoImage;
    IBProps.Scale = 0.5f;
    rndr::ui::DrawImageBox(IBProps);

    IBProps.BottomLeft = math::Point2{100, 0};
    IBProps.ImageId = ArrowDownImage;
    IBProps.Scale = 2.0f;
    rndr::ui::DrawImageBox(IBProps);

    rndr::ui::TextBoxProperties TBProps;
    TBProps.BaseLineStart = math::Point2{0, 100};
    TBProps.Font = EpilogueBoldFont;
    TBProps.Color = rndr::Colors::Black;
    rndr::ui::DrawTextBox("Learn more", TBProps);

    rndr::ui::EndFrame();
}

static rndr::ui::ImageId LoadImageAsset(const char* ImagePath)
{
    rndr::ui::ImageId Id = rndr::ui::AddImage(ImagePath);
    assert(Id != rndr::ui::kInvalidImageId);
    return Id;
}

static rndr::ui::FontHandle LoadFontAsset(const char* FontPath, int Size)
{
    rndr::ui::FontHandle Id = rndr::ui::AddFont(FontPath, Size);
    assert(Id != rndr::ui::kInvalidFontHandle);
    return Id;
}

void LoadAssets()
{
    ArrowUpImage = LoadImageAsset(FE_WORK_DIR "images/icon-arrow-up.png");
    ArrowDownImage = LoadImageAsset(FE_WORK_DIR "images/icon-arrow-down.png");
    CalendarImage = LoadImageAsset(FE_WORK_DIR "images/icon-calendar.png");
    CloseMenuImage = LoadImageAsset(FE_WORK_DIR "images/icon-close-menu.png");
    MenuImage = LoadImageAsset(FE_WORK_DIR "images/icon-menu.png");
    PlanningImage = LoadImageAsset(FE_WORK_DIR "images/icon-planning.png");
    RemindersImage = LoadImageAsset(FE_WORK_DIR "images/icon-reminders.png");
    TodoImage = LoadImageAsset(FE_WORK_DIR "images/icon-todo.png");
    LogoImage = LoadImageAsset(FE_WORK_DIR "images/logo.png");
    ClientAudiophileImage = LoadImageAsset(FE_WORK_DIR "images/client-audiophile.png");
    ClientDataBizImage = LoadImageAsset(FE_WORK_DIR "images/client-databiz.png");
    ClientMakerImage = LoadImageAsset(FE_WORK_DIR "images/client-maker.png");
    ClientMeetImage = LoadImageAsset(FE_WORK_DIR "images/client-meet.png");

    EpilogueMediumFont = LoadFontAsset(FE_WORK_DIR "fonts/Epilogue-Medium.ttf", kDefaultFontSize);
    EpilogueBoldFont = LoadFontAsset(FE_WORK_DIR "fonts/Epilogue-Bold.ttf", kDefaultFontSize);
}
