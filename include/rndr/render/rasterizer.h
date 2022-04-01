#pragma once

#include <vector>

#include "rndr/core/bounds2.h"
#include "rndr/core/math.h"

#include "rndr/render/pipeline.h"

namespace rndr
{

class Image;
class Model;
class Allocator;


#if RNDR_DEBUG

struct TriangleDebugInfo
{
    rndr::Point3r NDCPosition[3];
    rndr::Point3r ScreenPosition[3];
    rndr::Bounds2i OriginalBounds;
    rndr::Bounds2i ScreenBounds;
    bool bIsOutsideXY = false;
    bool bIsOutsideZ = false;
    bool bBackFace = false;
};

#endif

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
    void RunPixelShaders();

    void ProcessFragment(const Triangle& T, InFragmentInfo& InInfo);

private:
    const Pipeline* m_Pipeline = nullptr;
    std::vector<Triangle> m_Triangles;
    std::vector<OutVertexInfo> m_Vertices;
    std::vector<uint8_t> m_UserVertices;
    int m_InstanceCount;
    int m_VerticesPerInstanceCount;
    int m_TrianglePerInstanceCount;
    Model* m_Model;

    Allocator* m_ScratchAllocator = nullptr;

#if RNDR_DEBUG
    std::vector<TriangleDebugInfo> m_TriangleDebugInfos;
#endif
};

}  // namespace rndr