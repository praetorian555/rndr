#pragma once

#include <vector>

#include "rndr/core/math.h"
#include "rndr/render/pipeline.h"

namespace rndr
{

class Surface;
class Model;

class SoftwareRenderer
{
public:
    SoftwareRenderer(rndr::Surface* Surface);

    void SetPipeline(const rndr::Pipeline* Pipeline) { m_Pipeline = Pipeline; }

    void DrawTriangles(const std::vector<uint8_t>& VertexData,
                       int VertexDataStride,
                       const std::vector<int>& Indices,
                       void* InstanceData);
    void Draw(rndr::Model* Model, int InstanceCount = 1);

private:
    void DrawTriangle(const Point3r (&PositionsWithDepth)[3], void** VertexData);

private:
    Surface* m_Surface = nullptr;
    const Pipeline* m_Pipeline = nullptr;
};

}  // namespace rndr