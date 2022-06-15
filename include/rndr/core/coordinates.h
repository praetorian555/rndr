#pragma once

#include "math/point2.h"
#include "math/point3.h"

#include "rndr/core/base.h"

namespace rndr
{

// Discrete space and continuous space are to ways we can represent coordinates of pixels. Discrete
// space will use int values as coordinates while the continuous space will use real values as
// coordinates. In continuous space center of a pixel is at the offset (0.5, 0.5) and the pixel will
// start at (0, 0) and end at (1, 1).

struct PixelCoordinates
{
    static math::Point2 ToDiscreteSpace(const math::Point2& Point);
    static math::Point3 ToDiscreteSpace(const math::Point3& Point);

    static math::Point2 ToContinuousSpace(const math::Point2& Point);
    static math::Point3 ToContinuousSpace(const math::Point3& Point);
};

}  // namespace rndr
