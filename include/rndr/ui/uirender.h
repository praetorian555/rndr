#pragma once

#include "rndr/core/base.h"

#include "math/vector2.h"

namespace rndr
{
namespace ui
{

using RenderId = int;
static constexpr RenderId kInvalidRenderId = -1;

math::Vector2 GetRenderScreenSize();

}  // namespace ui
}  // namespace rndr
