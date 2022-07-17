#pragma once

#include "math/point2.h"
#include "math/vector2.h"

#include "rndr/ui/uirender.h"

namespace rndr
{
namespace ui
{

using ImageId = int;
static constexpr ImageId kInvalidImageId = -1;

ImageId AddImage(const char* ImagePath);
void RemoveImage(ImageId Id);

math::Vector2 GetImageSize(ImageId Id);
void GetImageTexCoords(ImageId Id, math::Point2* BottomLeft, math::Point2* TopRight);
RenderId GetImageRenderId(ImageId Id);

}  // namespace ui
}  // namespace rndr