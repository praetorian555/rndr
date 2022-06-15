#pragma once

#include "rndr/core/span.h"

#if defined RNDR_RASTER

#include "rndr/raster/rasterpipeline.h"

namespace rndr
{

class Model;
class Allocator;

/**
 * Renderer that uses rasterization to render. Rasterization is implemented fully on CPU side.
 */
class Rasterizer
{
public:
    Rasterizer();

    void SetPipeline(const rndr::Pipeline* Pipeline);

    void Draw(rndr::Model* Model);

    math::Point3 FromNDCToRasterSpace(const Point3& Point);
    math::Point3 FromRasterToNDCSpace(const Point3& Point);

private:
    void Setup(Model* Model);
    void RunVertexShaders();
    void SetupTriangles();
    void FindTrianglesToIgnore();
    void AllocateFragmentInfo();
    void BarycentricCoordinates();
    void SetupFragmentNeighbours();
    void RunFragmentShaders();

    void ProcessFragment(const Triangle& T, InFragmentInfo& InInfo);

    bool RunDepthTest(const math::Point2& PixelPosition, real NewDepth, real CurrentDepth, bool bIsEarly = false);

private:
    const Pipeline* m_Pipeline = nullptr;
    Span<Triangle> m_Triangles;
    Span<OutVertexInfo> m_Vertices;
    ByteSpan m_UserVertices;
    int m_InstanceCount;
    int m_VerticesPerInstanceCount;
    int m_TrianglePerInstanceCount;
    Model* m_Model;

    Allocator* m_ScratchAllocator = nullptr;
};

}  // namespace rndr

#endif  // RNDR_RASTER
