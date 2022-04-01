#pragma once

#include "rndr/core/span.h"
#include "rndr/core/math.h"

#include "rndr/render/shader.h"

namespace rndr
{

struct Cube
{
    static Span<Point3r> GetVertexPositions();
    static Span<Point2r> GetVertexTextureCoordinates();
    static Span<Normal3r> GetNormals();
    static IntSpan GetIndices();
};

}  // namespace rndr