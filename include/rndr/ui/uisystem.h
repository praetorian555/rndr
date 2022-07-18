#pragma once

#include "math/point2.h"
#include "math/vector2.h"
#include "math/vector4.h"

#include "rndr/core/base.h"
#include "rndr/core/colors.h"

#include "rndr/ui/uifont.h"
#include "rndr/ui/uiimage.h"
#include "rndr/ui/uirender.h"

namespace rndr
{

// Forward declarations
struct GraphicsContext;

namespace ui
{

static constexpr RenderId kWhiteImageRenderId = 0;

enum class PositionMode
{
    ViewportLeft,
    ViewportRight,
    ViewportTop,
    ViewportBottom,
    ViewportCenter,
    ParentLeft,
    ParentRight,
    ParentTop,
    ParentBottom,
    ParentCenter,
};

enum class SizeMode
{
    Absolute,  // Whatever is in the Size field is the final size of the box
    FitParent  // Sized same as the parent
};

struct BoxProperties
{
    PositionMode PositionModeX = PositionMode::ViewportLeft;
    PositionMode PositionModeY = PositionMode::ViewportBottom;

    // Bottom left point of the box in pixels. Origin of the UI scene is at (0, 0) in bottom left corner of the screen. It grows to the
    // right and upwards.
    math::Point2 BottomLeft;
    // Size of the box in pixels.
    math::Vector2 Size = math::Vector2{100, 100};
    // Tint of the box.
    math::Vector4 Color = rndr::Colors::Pink;

    float CornerRadius = 0.0f;
    float EdgeSoftness = 0.0f;
    float BorderThickness = 0.0f;
};

struct TextBoxProperties
{
    PositionMode PositionModeX = PositionMode::ViewportLeft;
    PositionMode PositionModeY = PositionMode::ViewportBottom;

    math::Point2 BaseLineStart;
    float Scale = 1.0f;
    math::Vector4 TextColor = Colors::White;
    math::Vector4 BackgroundColor = math::Vector4{0, 0, 0, 0};
    // FontId that you get by calling AddFont function.
    FontId Font = kInvalidFontId;
};

struct ImageBoxProperties
{
    PositionMode PositionModeX = PositionMode::ViewportLeft;
    PositionMode PositionModeY = PositionMode::ViewportBottom;

    math::Point2 BottomLeft;
    ImageId ImageId = kInvalidImageId;
    math::Vector4 Color = Colors::White;
    float Scale = 1.0f;
};

struct UIProperties
{
    int MaxInstanceCount = 1024;
    int MaxImageArraySize = 128;
    int MaxImageSideSize = 72 * 20;
    int MaxAtlasCount = 32;
    int MaxImageCount = 32;
};

bool Init(GraphicsContext* Context, const UIProperties& Props);
bool ShutDown();

void StartFrame();
void EndFrame();

void StartBox(const BoxProperties& Props);
void EndBox();

void DrawTextBox(const std::string& Text, const TextBoxProperties& Props);
void DrawImageBox(const ImageBoxProperties& Props);

void SetColor(const math::Vector4& Color);
void SetDim(const math::Point2& BottomLeft, const math::Vector2& Size);

bool MouseHovers();
bool LeftMouseButtonClicked();
bool RightMouseButtonClicked();

float GetViewportWidth();
float GetViewportHeight();

}  // namespace ui
}  // namespace rndr
