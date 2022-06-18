#pragma once

#include "math/point2.h"
#include "math/vector2.h"
#include "math/vector4.h"

#include "rndr/core/base.h"

namespace rndr
{

// Forward declarations
struct GraphicsContext;

namespace ui
{

struct Properties
{
};

struct BoxProperties
{
    math::Point2 BottomLeft;
    math::Vector2 Size;
    math::Vector4 Color;
};

bool Init(GraphicsContext* Context, const Properties& Props);
bool ShutDown();

void StartFrame();
void EndFrame();

void StartBox(const BoxProperties& Props);
void EndBox();

}  // namespace ui
}  // namespace rndr
