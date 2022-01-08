#pragma once

#include <functional>

#include "rndr/core/color.h"
#include "rndr/core/math.h"
#include "rndr/core/barycentric.h"

namespace rndr
{

struct PerPixelInfo
{
    Point2i Position; // In discrete space
    BarycentricCoordinates BarCoords;
    void* VertexData[3];
    void* Constants;
};

using PixelShaderCallback = std::function<Color(const PerPixelInfo&, real& DepthValue)>;

struct PixelShader
{
    bool bChangesDepth = false;
    PixelShaderCallback Callback;
};

struct PerVertexInfo
{
    int PrimitiveIndex;
    int VertexIndex;
    void* VertexData;    // Data specific for each vertex
    void* InstanceData;  // Data specific for each instance
    void* Constants;     // Data constant across all models and his instances
};

// Should return the point in NDC where x and y are in range [-1, 1] and z in range [0, 1]
using VertexShaderCallback = std::function<Point3r(const PerVertexInfo&)>;

struct VertexShader
{
    VertexShaderCallback Callback;
};

}  // namespace rndr