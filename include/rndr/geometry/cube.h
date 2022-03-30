#pragma once

#include <vector>

#include "rndr/core/math.h"
#include "rndr/render/shader.h"

namespace rndr
{

struct Cube
{
    static const std::vector<Point3r>& GetVertexPositions();
    static const std::vector<Point2r>& GetVertexTextureCoordinates();
    static const std::vector<Normal3r>& GetNormals();
    static const std::vector<int>& GetIndices();
};

}  // namespace rndr