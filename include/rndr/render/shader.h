#pragma once

#include <functional>

#include "rndr/core/color.h"
#include "rndr/core/math.h"

namespace rndr
{

struct PerPixelInfo
{
    Point2i Position;
    real Barycentric[3];
    void* VertexData[3];
};

using PixelShaderCallback = std::function<Color(const PerPixelInfo&)>;

struct PixelShader
{
    PixelShaderCallback Callback;
};

struct PerVertexInfo
{
    int PrimitiveIndex;
    int VertexIndex;
    void* VertexData;
    void* InstanceData;
};

using VertexShaderCallback = std::function<Point3r(const PerVertexInfo&)>;

struct VertexShader
{
    VertexShaderCallback Callback;
};

}  // namespace rndr