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

struct Triangle
{
    rndr::PerVertexInfo Vertices[3];
    rndr::Point3r Positions[3];
    real W[3];
    real OneOverW[3];
    real OneOverZ[3];
    rndr::Bounds2i Bounds{{0, 0}, {0, 0}};
    std::unique_ptr<rndr::BarycentricHelper> BarHelper;
    PerPixelInfo* Pixels = nullptr;
    bool bIgnore = false;

    PerPixelInfo& GetPixelInfo(int X, int Y)
    {
        assert(rndr::Inside(Point2i{X, Y}, Bounds));
        assert(Pixels);

        return Pixels[(X - Bounds.pMin.X) + (Y - Bounds.pMin.Y) * Bounds.Diagonal().X];
    }
};

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

    void Draw(rndr::Model* Model, int InstanceCount = 1);

    // Assumes valid depth values are in range [0, 1]
    static bool PerformDepthTest(rndr::DepthTest Operator, real Src, real Dst);

    Point3r FromNDCToRasterSpace(const Point3r& Point);
    Point3r FromRasterToNDCSpace(const Point3r& Point);

private:
    void RunVertexShadersAndSetupTriangles(Model* Model, int InstanceCount);
    void FindTrianglesToIgnore();
    void AllocatePixelInfo();
    void BarycentricCoordinates();
    void SetupFragmentNeighbours();
    void RunPixelShaders();

    void ProcessPixel(PerPixelInfo& PixelInfo, const Triangle& T);

    PerPixelInfo& GetPixelInfo(const Point2i& Position);
    PerPixelInfo& GetPixelInfo(int X, int Y);

private:
    const Pipeline* m_Pipeline = nullptr;
    std::vector<Triangle> m_Triangles;

    Allocator* m_ScratchAllocator = nullptr;

#if RNDR_DEBUG
    std::vector<TriangleDebugInfo> m_TriangleDebugInfos;
#endif
};

}  // namespace rndr