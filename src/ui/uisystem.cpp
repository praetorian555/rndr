#include "rndr/ui/uisystem.h"

#include <algorithm>
#include <vector>

#include "math/bounds2.h"
#include "math/point2.h"
#include "math/vector4.h"

#include "rndr/core/buffer.h"
#include "rndr/core/fileutils.h"
#include "rndr/core/framebuffer.h"
#include "rndr/core/graphicscontext.h"
#include "rndr/core/graphicstypes.h"
#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/pipeline.h"
#include "rndr/core/shader.h"

#include "rndr/ui/uibox.h"
#include "rndr/ui/uirender.h"

namespace rndr
{
namespace ui
{

static constexpr int kMaxInstances = 512;

UIProperties g_UIProps;

static std::vector<Box*> g_Stack;
static std::vector<Box*> g_Boxes;

static math::Point2 g_PrevMousePosition;
static math::Point2 g_MousePosition;
static bool g_PrevButtonState[3];
static bool g_ButtonState[3];
static int g_PrevScrollPosition = 0;
static int g_ScrollPosition = 0;

// Font module
bool InitFont();
void ShutDownFont();

// Render module
bool InitRender(GraphicsContext* Context);
void ShutDownRender();
void StartRenderFrame();
void EndRenderFrame(const Span<Box*> SortedBoxes);

// Image module
bool InitImage();
void ShutDownImage();

// Private functions
static void CleanupBoxes();
static void OnMouseMovement(InputPrimitive Primitive, InputTrigger Trigger, real Value);
static void OnButtonEvent(InputPrimitive Primitive, InputTrigger Trigger, real Value);
static void OnScroll(InputPrimitive Primitive, InputTrigger Trigger, real Value);

}  // namespace ui
}  // namespace rndr

bool rndr::ui::Init(GraphicsContext* Context, const UIProperties& Props)
{
    g_UIProps = Props;

    InitRender(Context);
    InitFont();
    InitImage();

    // TODO: Move this to separate function
    Box* ScreenBox = new Box();
    BoxProperties BoxProps;
    BoxProps.BottomLeft = math::Point2{0, 0};
    BoxProps.Size = GetRenderScreenSize();
    BoxProps.Color = math::Vector4{0, 0, 0, 0};
    ScreenBox->Props = BoxProps;
    ScreenBox->Parent = nullptr;
    ScreenBox->Level = 0;
    ScreenBox->Bounds = math::Bounds2();
    g_Stack.push_back(ScreenBox);

    // Move this to separate function
    rndr::InputContext* InputContext = rndr::InputSystem::Get()->GetContext();
    InputContext->CreateMapping("MouseMovement", OnMouseMovement);
    InputContext->AddBinding("MouseMovement", rndr::InputPrimitive::Mouse_AxisX, rndr::InputTrigger::AxisChangedAbsolute);
    InputContext->AddBinding("MouseMovement", rndr::InputPrimitive::Mouse_AxisY, rndr::InputTrigger::AxisChangedAbsolute);
    InputContext->CreateMapping("ButtonAction", OnButtonEvent);
    InputContext->AddBinding("ButtonAction", rndr::InputPrimitive::Mouse_LeftButton, rndr::InputTrigger::ButtonDown);
    InputContext->AddBinding("ButtonAction", rndr::InputPrimitive::Mouse_LeftButton, rndr::InputTrigger::ButtonUp);
    InputContext->AddBinding("ButtonAction", rndr::InputPrimitive::Mouse_RightButton, rndr::InputTrigger::ButtonDown);
    InputContext->AddBinding("ButtonAction", rndr::InputPrimitive::Mouse_RightButton, rndr::InputTrigger::ButtonUp);
    InputContext->CreateMapping("Scroll", OnScroll);
    InputContext->AddBinding("Scroll", rndr::InputPrimitive::Mouse_AxisWheel, rndr::InputTrigger::AxisChangedAbsolute);

    return true;
}

bool rndr::ui::ShutDown()
{
    ShutDownImage();
    ShutDownFont();
    ShutDownRender();

    return true;
}

void rndr::ui::StartFrame()
{
    StartRenderFrame();

    // TODO: This might cause chain reaction that changes the size and the position of other boxes
    const math::Vector2 ScreenSize = GetRenderScreenSize();
    g_Stack[0]->Props.Size = ScreenSize;
    g_Stack[0]->Bounds = math::Bounds2(math::Point2{}, math::Point2{} + ScreenSize);

    g_Boxes.push_back(g_Stack[0]);
}

void rndr::ui::EndFrame()
{
    auto Compare = [](const Box* A, const Box* B) { return A->Level < B->Level; };
    std::sort(g_Boxes.begin(), g_Boxes.end(), Compare);

    EndRenderFrame(Span<Box*>(g_Boxes));

    CleanupBoxes();

    // TODO: Move this to separate function
    for (int i = 0; i < 3; i++)
    {
        g_PrevButtonState[i] = g_ButtonState[i];
    }
}

static float CalculatePosition(rndr::ui::PositionMode Mode, rndr::ui::Box* Parent, float Offset, int ValueIndex)
{
    const math::Vector2 ScreenSize = rndr::ui::GetRenderScreenSize();

    math::Point2 Ref;

    float Result = 0.0f;
    switch (Mode)
    {
        case rndr::ui::PositionMode::ViewportTop:
        {
            Ref.Y = ScreenSize.Y;
            break;
        }
        case rndr::ui::PositionMode::ViewportRight:
        {
            Ref.X = ScreenSize.X;
            break;
        }
        case rndr::ui::PositionMode::ViewportBottom:
        case rndr::ui::PositionMode::ViewportLeft:
        {
            break;
        }
        case rndr::ui::PositionMode::ViewportCenter:
        {
            Ref.X = ScreenSize.X / 2;
            Ref.Y = ScreenSize.Y / 2;
            break;
        }
        case rndr::ui::PositionMode::ParentTop:
        {
            if (Parent)
            {
                Ref = Parent->Props.BottomLeft;
                Ref.Y += Parent->Props.Size.Y;
            }
            break;
        }
        case rndr::ui::PositionMode::ParentLeft:
        case rndr::ui::PositionMode::ParentBottom:
        {
            if (Parent)
            {
                Ref = Parent->Props.BottomLeft;
            }
            break;
        }
        case rndr::ui::PositionMode::ParentRight:
        {
            if (Parent)
            {
                Ref = Parent->Props.BottomLeft;
                Ref.X += Parent->Props.Size.X;
            }
            break;
        }
        case rndr::ui::PositionMode::ParentCenter:
        {
            Ref = Parent->Props.BottomLeft;
            Ref.X += Parent->Props.Size.X / 2;
            Ref.Y += Parent->Props.Size.Y / 2;
            break;
        }
        default:
        {
            assert(false);
            break;
        }
    }

    Result = Ref.Data[ValueIndex] + Offset;
    return Result;
}

static bool IsPositionModeCenter(rndr::ui::PositionMode Mode)
{
    return Mode == rndr::ui::PositionMode::ParentCenter || Mode == rndr::ui::PositionMode::ViewportCenter;
}

static bool IsPositionModeX(rndr::ui::PositionMode Mode)
{
    return Mode == rndr::ui::PositionMode::ParentLeft || Mode == rndr::ui::PositionMode::ParentRight ||
           Mode == rndr::ui::PositionMode::ParentCenter || Mode == rndr::ui::PositionMode::ViewportLeft ||
           Mode == rndr::ui::PositionMode::ViewportRight || Mode == rndr::ui::PositionMode::ViewportCenter;
}

static bool IsPositionModeY(rndr::ui::PositionMode Mode)
{
    return Mode == rndr::ui::PositionMode::ParentTop || Mode == rndr::ui::PositionMode::ParentBottom ||
           Mode == rndr::ui::PositionMode::ParentCenter || Mode == rndr::ui::PositionMode::ViewportTop ||
           Mode == rndr::ui::PositionMode::ViewportBottom || Mode == rndr::ui::PositionMode::ViewportCenter;
}

void rndr::ui::StartBox(const BoxProperties& Props)
{
    assert(IsPositionModeX(Props.PositionModeX));
    assert(IsPositionModeY(Props.PositionModeY));

    Box* Parent = g_Stack.back();
    math::Vector2 Offset;
    Offset.X = IsPositionModeCenter(Props.PositionModeX) ? -Props.Size.X / 2 : Props.BottomLeft.X;
    Offset.Y = IsPositionModeCenter(Props.PositionModeY) ? -Props.Size.Y / 2 : Props.BottomLeft.Y;
    math::Point2 BottomLeft;
    BottomLeft.X = CalculatePosition(Props.PositionModeX, Parent, Offset.X, 0);
    BottomLeft.Y = CalculatePosition(Props.PositionModeY, Parent, Offset.Y, 1);

    assert(g_Boxes.size() < kMaxInstances);
    Box* B = new Box();
    B->Props = Props;
    B->Props.BottomLeft = BottomLeft;
    B->Parent = Parent;
    B->Parent->Children.push_back(B);
    B->Level = B->Parent->Level + 1;
    B->Bounds = math::Bounds2(Props.BottomLeft, Props.BottomLeft + Props.Size);
    g_Stack.push_back(B);
    g_Boxes.push_back(B);
}

void rndr::ui::EndBox()
{
    assert(g_Stack.size() > 1);
    g_Stack.pop_back();
}

void rndr::ui::DrawTextBox(const std::string& Text, const TextBoxProperties& Props)
{
    assert(IsPositionModeX(Props.PositionModeX));
    assert(IsPositionModeY(Props.PositionModeY));

    if (!ContainsFont(Props.Font))
    {
        RNDR_LOG_ERROR("Invalid font!");
        return;
    }

    Box* Parent = new Box{};
    Parent->Props.Color = Props.BackgroundColor;
    Parent->Parent = g_Stack.back();
    Parent->Parent->Children.push_back(Parent);
    Parent->Level = Parent->Parent->Level + 1;
    Parent->RenderId = kWhiteImageRenderId;
    g_Boxes.push_back(Parent);

    const int VerticalAdvance = GetFontVerticalAdvance(Props.Font);
    const float Descent = Props.Scale * GetFontDescent(Props.Font);

    const bool bCenterX = IsPositionModeCenter(Props.PositionModeX);
    const bool bCenterY = IsPositionModeCenter(Props.PositionModeY);

    math::Point2 StartPos = Props.BaseLineStart;
    if (bCenterX)
    {
        StartPos.X = 0;
    }
    else
    {
        StartPos.X = CalculatePosition(Props.PositionModeX, Parent->Parent, StartPos.X, 0);
    }
    if (bCenterY)
    {
        StartPos.Y = 0;
    }
    else
    {
        StartPos.Y = CalculatePosition(Props.PositionModeY, Parent->Parent, StartPos.Y, 1);
    }
    const math::Point2 OriginalStartPos = StartPos;

    Parent->Props.BottomLeft.X = StartPos.X;
    Parent->Props.BottomLeft.Y = StartPos.Y + Descent;
    Parent->Props.Size.X = 0;
    Parent->Props.Size.Y = Props.Scale * GetFontSize(Props.Font);
    for (int i = 0; i < Text.size(); i++)
    {
        int Codepoint = Text[i];

        if (Codepoint == '\n')
        {
            if (Parent->Props.Size.X < StartPos.X - OriginalStartPos.X)
            {
                Parent->Props.Size.X = StartPos.X - OriginalStartPos.X;
            }
            StartPos.X = OriginalStartPos.X;
            StartPos.Y -= Props.Scale * VerticalAdvance;
            continue;
        }

        const int GlyphAdvance = GetGlyphAdvance(Props.Font, Codepoint, Text[i + 1]);

        if (Codepoint == ' ')
        {
            StartPos.X += Props.Scale * GlyphAdvance;
            continue;
        }

        math::Vector2 GlyphSize = GetGlyphSize(Props.Font, Codepoint);
        GlyphSize *= Props.Scale;

        math::Vector2 GlyphBearing = GetGlyphBearing(Props.Font, Codepoint);
        GlyphBearing *= Props.Scale;

        assert(g_Boxes.size() < kMaxInstances);
        Box* B = new Box();
        BoxProperties BoxProps;
        BoxProps.BottomLeft = StartPos + GlyphBearing;
        BoxProps.Size = GlyphSize;
        BoxProps.Color = Props.TextColor;
        B->Props = BoxProps;
        B->Parent = Parent;
        B->Parent->Children.push_back(B);
        B->Level = B->Parent->Level + 1;
        B->Bounds = math::Bounds2(BoxProps.BottomLeft, BoxProps.BottomLeft + BoxProps.Size);
        B->RenderId = GetFontRenderId(Props.Font);
        GetGlyphTexCoords(Props.Font, Codepoint, &B->TexCoordsBottomLeft, &B->TexCoordsTopRight);
        g_Boxes.push_back(B);

        StartPos.X += Props.Scale * GlyphAdvance;
    }

    if (Parent->Props.Size.X < StartPos.X - OriginalStartPos.X)
    {
        Parent->Props.Size.X = StartPos.X - OriginalStartPos.X;
    }

    Parent->Bounds = math::Bounds2(Parent->Props.BottomLeft, Parent->Props.BottomLeft + Parent->Props.Size);

    if (bCenterX || bCenterY)
    {
        math::Point2 BottomLeft;
        if (bCenterX)
        {
            const float Offset = -Parent->Props.Size.X / 2;
            BottomLeft.X = CalculatePosition(Props.PositionModeX, Parent->Parent, Offset, 0);
        }
        if (bCenterY)
        {
            const float Offset = -Parent->Props.Size.Y / 2;
            BottomLeft.Y = CalculatePosition(Props.PositionModeY, Parent->Parent, Offset, 1);
            BottomLeft.Y -= Descent;
        }

        for (Box* Child : Parent->Children)
        {
            Child->Props.BottomLeft += BottomLeft;
        }
    }
}

void rndr::ui::DrawImageBox(const ImageBoxProperties& Props)
{
    assert(IsPositionModeX(Props.PositionModeX));
    assert(IsPositionModeY(Props.PositionModeY));

    Box* Parent = g_Stack.back();
    const math::Vector2 ImageSize = Props.Scale * GetImageSize(Props.ImageId);

    math::Vector2 Offset;
    Offset.X = IsPositionModeCenter(Props.PositionModeX) ? -ImageSize.X / 2 : Props.BottomLeft.X;
    Offset.Y = IsPositionModeCenter(Props.PositionModeY) ? -ImageSize.Y / 2 : Props.BottomLeft.Y;

    math::Point2 BottomLeft;
    BottomLeft.X = CalculatePosition(Props.PositionModeX, Parent, Offset.X, 0);
    BottomLeft.Y = CalculatePosition(Props.PositionModeY, Parent, Offset.Y, 1);

    assert(g_Boxes.size() < kMaxInstances);
    Box* B = new Box();
    BoxProperties BoxProps;
    BoxProps.BottomLeft = BottomLeft;
    BoxProps.Size = ImageSize;
    BoxProps.Color = Props.Color;
    B->Props = BoxProps;
    B->Parent = Parent;
    B->Parent->Children.push_back(B);
    B->Level = B->Parent->Level + 1;
    B->Bounds = math::Bounds2(BoxProps.BottomLeft, BoxProps.BottomLeft + BoxProps.Size);
    B->RenderId = GetImageRenderId(Props.ImageId);
    GetImageTexCoords(Props.ImageId, &B->TexCoordsBottomLeft, &B->TexCoordsTopRight);
    g_Boxes.push_back(B);
}

void rndr::ui::SetColor(const math::Vector4& Color)
{
    Box* B = g_Stack.back();
    B->Props.Color = Color;
}

void rndr::ui::SetDim(const math::Point2& BottomLeft, const math::Vector2& Size)
{
    Box* B = g_Stack.back();
    B->Props.BottomLeft = BottomLeft;
    B->Props.Size = Size;
    B->Bounds = math::Bounds2(BottomLeft, BottomLeft + Size);
}

float rndr::ui::GetViewportWidth()
{
    return GetRenderScreenSize().X;
}

float rndr::ui::GetViewportHeight()
{
    return GetRenderScreenSize().Y;
}

bool rndr::ui::MouseHovers()
{
    Box* B = g_Stack.back();
    assert(B);

    const math::Bounds2 BoxBounds(B->Props.BottomLeft, B->Props.BottomLeft + B->Props.Size);
    return math::InsideInclusive(g_MousePosition, BoxBounds);
}

bool rndr::ui::LeftMouseButtonClicked()
{
    const int Index = 0;
    return MouseHovers() && g_PrevButtonState[Index] && !g_ButtonState[Index];
}

bool rndr::ui::RightMouseButtonClicked()
{
    const int Index = (int)InputPrimitive::Mouse_RightButton - (int)InputPrimitive::Mouse_LeftButton;
    return g_PrevButtonState[Index] && !g_ButtonState[Index];
}

void rndr::ui::CleanupBoxes()
{
    g_Boxes[0] = nullptr;
    for (Box* B : g_Boxes)
    {
        delete B;
    }
    g_Boxes.clear();

    while (g_Stack.size() > 1)
        g_Stack.pop_back();
}

void rndr::ui::OnMouseMovement(InputPrimitive Primitive, InputTrigger Trigger, real Value)
{
    if (Primitive == InputPrimitive::Mouse_AxisX)
    {
        g_PrevMousePosition.X = g_MousePosition.X;
        g_MousePosition.X = Value;
    }
    else if (Primitive == InputPrimitive::Mouse_AxisY)
    {
        g_PrevMousePosition.Y = g_MousePosition.Y;
        g_MousePosition.Y = Value;
    }
    else
    {
        assert(false);
    }
}

void rndr::ui::OnButtonEvent(InputPrimitive Primitive, InputTrigger Trigger, real Value)
{
    // TODO(mkostic): Currently this doesn't handle multiple edges in one frame
    int Index = (int)Primitive - (int)InputPrimitive::Mouse_LeftButton;
    g_ButtonState[Index] = Trigger == InputTrigger::ButtonUp;
}

void rndr::ui::OnScroll(InputPrimitive Primitive, InputTrigger Trigger, real Value)
{
    g_PrevScrollPosition = g_ScrollPosition;
    g_ScrollPosition += Value;
}
