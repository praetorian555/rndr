#include "rndr/raster/rastersampler.h"

#if defined RNDR_RASTER

rndr::Sampler2D::Sampler2D(Image* Image) : m_Image(Image) {}

rndr::Vector4r rndr::Sampler2D::Sample(const Point2r& TexCoord, const Vector2r& duvdx, const Vector2r& duvdy)
{
    // TODO(mkostic): Rebase uv to be in range [0, 1]

    const real Width = std::max(std::max(std::abs(duvdx.X), std::abs(duvdx.Y)), std::max(std::abs(duvdy.X), std::abs(duvdy.Y)));

    const int MipMapLevels = m_Image->m_MipMaps.size();
    real LOD = MipMapLevels - 1 + Log2(std::max(Width, (real)1e-8));
    LOD += m_Image->m_Config.LODBias;

    const ImageFiltering Filter = LOD < 0 ? m_Image->m_Config.MagFilter : m_Image->m_Config.MinFilter;

    Vector4r Result;
    switch (Filter)
    {
        case ImageFiltering::NearestNeighbor:
        {
            Result = SampleNearestNeighbor(m_Image, TexCoord);
            break;
        }
        case ImageFiltering::BilinearInterpolation:
        {
            Result = SampleBilinear(m_Image, TexCoord);
            break;
        }
        case ImageFiltering::TrilinearInterpolation:
        {
            assert(LOD >= 0);  // Not allowed for magnification filters
            Result = SampleTrilinear(m_Image, TexCoord, LOD);
            break;
        }
    }

    return Result;
}

rndr::Vector4r rndr::Sampler2D::SampleNearestNeighbor(const Image* I, const Point2r& TexCoord)
{
    const real U = TexCoord.X;
    const real V = TexCoord.Y;

    const real X = (I->m_Config.Width - 1) * U;
    const real Y = (I->m_Config.Height - 1) * V;

    const rndr::Point2i NearestDesc{(int)X, (int)Y};
    return I->GetPixelColor(NearestDesc);
}

rndr::Vector4r rndr::Sampler2D::SampleBilinear(const Image* I, const Point2r& TexCoord)
{
    const real U = TexCoord.X;
    const real V = TexCoord.Y;

    const real X = (I->m_Config.Width - 1) * U;
    const real Y = (I->m_Config.Height - 1) * V;

    const Point2i BottomLeft{(int)(X - 0.5), (int)(Y - 0.5)};
    const Point2i BottomRight{(int)(X + 0.5), (int)(Y - 0.5)};
    const Point2i TopLeft{(int)(X - 0.5), (int)(Y + 0.5)};
    const Point2i TopRight{(int)(X + 0.5), (int)(Y + 0.5)};

    Vector4r Result;
    // clang-format off
    Result = I->GetPixelColor(BottomLeft)  * (1 - U) * (1 - V) +
             I->GetPixelColor(BottomRight) *      U  * (1 - V) +
             I->GetPixelColor(TopLeft)     * (1 - U) *      V  +
             I->GetPixelColor(TopRight)    *      U  *      V;
    // clang-format on

    return Result;
}

rndr::Vector4r rndr::Sampler2D::SampleTrilinear(const Image* I, const Point2r& TexCoord, real LOD)
{
    const int Floor = (int)LOD;
    int Ceil = (int)(LOD + 1);

    if (Ceil == I->m_MipMaps.size())
    {
        Ceil = Floor;
    }

    const Vector4r FloorSample = SampleBilinear(I->m_MipMaps[Floor], TexCoord);
    const Vector4r CeilSample = SampleBilinear(I->m_MipMaps[Ceil], TexCoord);

    return rndr::Lerp(LOD - (real)Floor, FloorSample, CeilSample);
}

#endif  // RNDR_RASTER
