#pragma once

#include <functional>

#include "rndr/color.h"
#include "rndr/core/math.h"

namespace rndr
{

struct PerPixelInfo
{
    Point2i Position;
    real Barycentric[3];
};

using PixelShaderCallback = std::function<Color(const PerPixelInfo&)>;

struct PixelShader
{
    PixelShaderCallback Callback;
};

struct PerVertexInfo
{
    Point3r Position;
    int PrimitiveIndex;
    int VertexIndex;
};

using VertexShaderCallback = std::function<Point3r(const PerVertexInfo&)>;

struct VertexShader
{
    VertexShaderCallback Callback;
};

}  // namespace rndr