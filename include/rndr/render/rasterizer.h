#pragma once

#include "rndr/core/bounds2.h"
#include "rndr/core/math.h"
#include "rndr/core/span.h"

#include "rndr/render/rasterpipeline.h"

namespace rndr
{

class Image;
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

    // Assumes valid depth values are in range [0, 1]
    static bool PerformDepthTest(rndr::DepthTest Operator, real Src, real Dst);

    Point3r FromNDCToRasterSpace(const Point3r& Point);
    Point3r FromRasterToNDCSpace(const Point3r& Point);

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

private:
    const Pipeline* m_Pipeline = nullptr;
    rndr::Span<Triangle> m_Triangles;
    rndr::Span<OutVertexInfo> m_Vertices;
    rndr::ByteSpan m_UserVertices;
    int m_InstanceCount;
    int m_VerticesPerInstanceCount;
    int m_TrianglePerInstanceCount;
    Model* m_Model;

    Allocator* m_ScratchAllocator = nullptr;
};

}  // namespace rndr