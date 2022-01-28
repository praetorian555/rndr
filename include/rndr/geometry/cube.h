#pragma once

#include <vector>

#include "rndr/core/math.h"

namespace rndr
{

struct CubeVertexData
{
    rndr::Point3r Position;
    rndr::Point2r TextureCoords;
};

struct Cube
{
    static const std::vector<CubeVertexData>& GetVertices();
    static const std::vector<int>& GetIndices();
};

}  // namespace rndr