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
    Absolute,
    ParentRelative
};

enum class SizeMode
{
    Absolute,     // Whatever is in the Size field is the final size of the box
    FitChildren,  // Sized so that it encompases all children
    FitParent     // Sized same as the parent
};

struct BoxProperties
{
    // Bottom left point of the box in pixels. Origin of the UI scene is at (0, 0) in bottom left corner of the screen. It grows to the
    // right and upwards.
    math::Point2 BottomLeft;
    // Size of the box in pixels.
    math::Vector2 Size = math::Vector2{100, 100};
    // Tint of the box.
    math::Vector4 Color = rndr::Colors::Pink;
};

struct TextBoxProperties
{
    math::Point2 BaseLineStart;
    float Scale = 1.0f;
    math::Vector4 Color = Colors::White;
    // FontHandle that you get by calling AddFont function.
    FontHandle Font = kInvalidFontHandle;
};

struct ImageBoxProperties
{
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

}  // namespace ui
}  // namespace rndr
