#pragma once

#include "math/point2.h"
#include "math/vector2.h"
#include "math/vector4.h"

#include "rndr/core/base.h"

#include "rndr/ui/uifont.h"
#include "rndr/ui/uirender.h"

namespace rndr
{

// Forward declarations
struct GraphicsContext;

namespace ui
{

static constexpr RenderId kWhiteImageRenderId = 0;


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
    float Scale;
    math::Vector4 Color = rndr::Colors::Pink;
    // FontHandle that you get by calling AddFont function.
    FontHandle Font = kInvalidFontHandle;
};

struct UIProperties
{
    int MaxInstanceCount = 1024;
    int MaxImageArraySize = 32;
    int MaxImageSideSize = 72 * 20;
    int MaxAtlasCount = 32;
};

bool Init(GraphicsContext* Context, const UIProperties& Props);
bool ShutDown();

void StartFrame();
void EndFrame();

void StartBox(const BoxProperties& Props);
void EndBox();

void DrawTextBox(const std::string& Text, const TextBoxProperties& Props);

void SetColor(const math::Vector4& Color);
void SetDim(const math::Point2& BottomLeft, const math::Vector2& Size);

bool MouseHovers();
bool LeftMouseButtonClicked();
bool RightMouseButtonClicked();

}  // namespace ui
}  // namespace rndr
