#pragma once

#include <vector>

#include "rndr/core/math.h"
#include "rndr/render/pipeline.h"

namespace rndr
{

class Surface;

class SoftwareRenderer
{
public:
    SoftwareRenderer(rndr::Surface* Surface);

    void SetPipeline(rndr::Pipeline* Pipeline) { m_Pipeline = Pipeline; }

    void DrawTriangles(const std::vector<Point3r>& Positions,
                       const std::vector<int>& Indices);

private:
    void DrawTriangle(const Point3r (&PositionsWithDepth)[3]);

private:
    Surface* m_Surface = nullptr;
    Pipeline* m_Pipeline = nullptr;
};

}  // namespace rndr