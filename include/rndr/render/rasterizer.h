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
struct Triangle;

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
    void ProcessPixel(PerPixelInfo& PixelInfo, const Triangle& T);

    PerPixelInfo& GetPixelInfo(const Point2i& Position);
    PerPixelInfo& GetPixelInfo(int X, int Y);

private:
    const Pipeline* m_Pipeline = nullptr;

    Allocator* m_ScratchAllocator = nullptr;
};

}  // namespace rndr