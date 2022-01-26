#pragma once

#include <vector>

#include "rndr/core/bounds2.h"
#include "rndr/core/math.h"

#include "rndr/render/pipeline.h"

struct VertexShaderExecutor;

namespace rndr
{

class Image;
class Model;

struct Triangle
{
    PerVertexInfo Vertices[3];
    Point3r Positions[3];
    Bounds2i Bounds{{0, 0}, {0, 0}};
    std::unique_ptr<BarycentricHelper> BarHelper;
};

/**
 * Renderer that uses rasterization to render. Rasterization is implemented fully on CPU side.
 */
class Rasterizer
{
public:
    Rasterizer() = default;

    void SetPipeline(const rndr::Pipeline* Pipeline);

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

    void ProcessPixel(const PerPixelInfo& PixelInfo, const Triangle& T);

private:
    friend struct VertexShaderExecutor;

private:
    const Pipeline* m_Pipeline = nullptr;
    std::vector<Triangle*> Triangles;
};

}  // namespace rndr