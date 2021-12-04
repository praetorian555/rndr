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
    void* Constants;
};

using PixelShaderCallback = std::function<Color(const PerPixelInfo&)>;

struct PixelShader
{
    bool bChangesDepth = false;
    PixelShaderCallback Callback;
};

struct PerVertexInfo
{
    int PrimitiveIndex;
    int VertexIndex;
    void* VertexData; // Data specific for each vertex
    void* InstanceData; // Data specific for each instance
    void* Constants; // Data constant across all models and his instances
};

using VertexShaderCallback = std::function<Point3r(const PerVertexInfo&)>;

struct VertexShader
{
    VertexShaderCallback Callback;
};

}  // namespace rndr