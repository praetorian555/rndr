#pragma once

// TODO: Find smaller alternative
#include <vector>

#include "math/bounds2.h"

#include "rndr/ui/uisystem.h"

namespace rndr
{
namespace ui
{

struct Box
{
    BoxProperties Props;
    Box* Parent;
    std::vector<Box*> Children;

    int Level;
    math::Bounds2 Bounds;
    
    // Used only when box should display an image
    RenderId RenderId = kWhiteImageRenderId;
    math::Point2 TexCoordsBottomLeft = math::Point2{0, 0};
    math::Point2 TexCoordsTopRight = math::Point2{1, 1};
};

}  // namespace ui
}  // namespace rndr
