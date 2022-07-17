#include "rndr/core/rndrapp.h"
#include "rndr/core/setup.h"

#include "rndr/ui/uisystem.h"

class rndr::GraphicsContext;
class rndr::Window;

static void LoadAssets();

static void AppLoop(float DeltaSeconds);

static void DrawNavBar(float DeltaSeconds);

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

rndr::ui::FontId EpilogueMediumFont;
rndr::ui::FontId EpilogueBoldFont;

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

static rndr::ui::ImageId LoadImageAsset(const char* ImagePath)
{
    rndr::ui::ImageId Id = rndr::ui::AddImage(ImagePath);
    assert(Id != rndr::ui::kInvalidImageId);
    return Id;
}

static rndr::ui::FontId LoadFontAsset(const char* FontPath, int Size)
{
    rndr::ui::FontId Id = rndr::ui::AddFont(FontPath, Size);
    assert(Id != rndr::ui::kInvalidFontId);
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

void AppLoop(float DeltaSeconds)
{
    rndr::ui::StartFrame();

    DrawNavBar(DeltaSeconds);

    rndr::ui::EndFrame();
}

static void DrawMenuText(const char* Text, float Offset)
{
    rndr::ui::TextBoxProperties TextProps;
    TextProps.PositionModeX = rndr::ui::PositionMode::ParentLeft;
    TextProps.PositionModeY = rndr::ui::PositionMode::ParentCenter;
    TextProps.BaseLineStart = math::Point2{Offset, 10};
    TextProps.Scale = 1.0f;
    TextProps.Font = EpilogueMediumFont;
    TextProps.TextColor = math::Vector4{0, 0, 0, 1.0};
    rndr::ui::DrawTextBox(Text, TextProps);
}

void DrawNavBar(float DeltaSeconds)
{
    rndr::ui::BoxProperties NavBarProps;
    NavBarProps.Size.X = rndr::ui::GetViewportWidth();
    NavBarProps.Size.Y = 80;
    NavBarProps.PositionModeX = rndr::ui::PositionMode::ViewportLeft;
    NavBarProps.PositionModeY = rndr::ui::PositionMode::ViewportTop;
    NavBarProps.BottomLeft = math::Point2{0, -NavBarProps.Size.Y};
    NavBarProps.Color = rndr::Colors::White;

    rndr::ui::StartBox(NavBarProps);

    rndr::ui::ImageBoxProperties LogoProps;
    LogoProps.PositionModeX = rndr::ui::PositionMode::ParentLeft;
    LogoProps.PositionModeY = rndr::ui::PositionMode::ParentCenter;
    LogoProps.BottomLeft = math::Point2{30, 25};
    LogoProps.Scale = 1.0f;
    LogoProps.ImageId = LogoImage;

    rndr::ui::DrawImageBox(LogoProps);

    DrawMenuText("Features", 150);
    DrawMenuText("Companies", 250);
    DrawMenuText("Careers", 350);
    DrawMenuText("About", 450);

    rndr::ui::TextBoxProperties ButtonProps;
    ButtonProps.PositionModeX = rndr::ui::PositionMode::ParentRight;
    ButtonProps.PositionModeY = rndr::ui::PositionMode::ParentCenter;
    ButtonProps.BaseLineStart = math::Point2{-100, 0};
    ButtonProps.Font = EpilogueMediumFont;
    ButtonProps.TextColor = rndr::Colors::Black;

    rndr::ui::DrawTextBox("Register", ButtonProps);

    ButtonProps.BaseLineStart = math::Point2{-200, 0};

    rndr::ui::DrawTextBox("Login", ButtonProps);

    rndr::ui::EndBox();
}
