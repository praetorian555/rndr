#pragma once

#include "rndr/core/math.h"

namespace rndr
{

// Discrete space and continuous space are to ways we can represent coordinates of pixels. Discrete
// space will use int values as coordinates while the continuous space will use real values as
// coordinates. In continuous space center of a pixel is at the offset (0.5, 0.5) and the pixel will
// start at (0, 0) and end at (1, 1).

struct PixelCoordinates
{
    static Point2i ToDiscreteSpace(const Point2r& Point);
    static Point3i ToDiscreteSpace(const Point3r& Point);

    static Point2r ToContinuousSpace(const Point2i& Point);
    static Point3r ToContinuousSpace(const Point3i& Point);
};

}  // namespace rndr