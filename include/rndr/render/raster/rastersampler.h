#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER

#include "rndr/core/math.h"

#include "rndr/raster/rasterimage.h"

namespace rndr
{

class Sampler2D
{
public:
    Sampler2D(Image* Image);

    math::Vector4Sample(const Point2r& TexCoord, const math::Vector2& duvdx, const math::Vector2& duvdy);

    Image* GetImage() { return m_Image; }
    operator bool() const { return m_Image != nullptr; }

private:
    math::Vector4SampleNearestNeighbor(const Image* I, const Point2r& TexCoord);
    math::Vector4SampleBilinear(const Image* I, const Point2r& TexCoord);
    math::Vector4SampleTrilinear(const Image* I, const Point2r& TexCoord, real LOD);

    Point2r Wrap(const Point2r& TexCoords) const;
    real Wrap(real Value, ImageWrapping WrapMethod) const;

private:
    Image* m_Image;
};

}  // namespace rndr

#endif  // RNDR_RASTER