#pragma once

#include <vector>

#include "rndr/core/math.h"
#include "rndr/render/pipeline.h"

namespace rndr
{

class Image;
class Model;

/**
 * Renderer that uses rasterization to render. Rasterization is implemented fully on CPU side.
 */
class Rasterizer
{
public:
    Rasterizer() = default;

    void SetPipeline(const rndr::Pipeline* Pipeline) { m_Pipeline = Pipeline; }

    void Draw(rndr::Model* Model, int InstanceCount = 1);

private:
    void DrawTriangles(void* Constants,
                       const std::vector<uint8_t>& VertexData,
                       int VertexDataStride,
                       const std::vector<int>& Indices,
                       void* InstanceData);
    void DrawTriangle(void* Constants, const Point3r (&PositionsWithDepth)[3], void** VertexData);
    bool RunDepthTest(const Point2i& PixelPosition, real NewDepthValue);
    Color ApplyAlphaCompositing(const Point2i& PixelPosition, Color NewValue);
    Point3r FromNDCToRasterSpace(const Point3r& Point);
    Point3r FromRasterToNDCSpace(const Point3r& Point);

private:
    const Pipeline* m_Pipeline = nullptr;
};

}  // namespace rndr