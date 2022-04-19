#include "rndr/raster/rastersampler.h"

#if defined RNDR_RASTER

rndr::Sampler2D::Sampler2D(Image* Image) : m_Image(Image) {}

rndr::Vector4r rndr::Sampler2D::Sample(const Point2r& TC, const Vector2r& duvdx, const Vector2r& duvdy)
{
    Point2r TexCoords = Wrap(TC);
    // Special case, we use Inifinity to signal that we need to wrap coordinates with specified border color
    if (TexCoords.X == Infinity || TexCoords.Y == Infinity)
    {
        return m_Image->m_Props.WrapBorderColor;
    }

    const real Width = std::max(std::max(std::abs(duvdx.X), std::abs(duvdx.Y)), std::max(std::abs(duvdy.X), std::abs(duvdy.Y)));

    const int MipMapLevels = m_Image->m_MipMaps.size();
    real LOD = MipMapLevels - 1 + Log2(std::max(Width, (real)1e-8));
    LOD += m_Image->m_Props.LODBias;

    const ImageFiltering Filter = LOD < 0 ? m_Image->m_Props.MagFilter : m_Image->m_Props.MinFilter;
    bool bIsMin = LOD >= 0;

    Vector4r Result;
    if (bIsMin && m_Image->m_Props.bUseMips)
    {
        Result = SampleTrilinear(m_Image, TexCoords, LOD);
    }
    else
    {
        switch (Filter)
        {
            case ImageFiltering::Point:
            {
                Result = SampleNearestNeighbor(m_Image, TexCoords);
                break;
            }
            case ImageFiltering::Linear:
            {
                Result = SampleBilinear(m_Image, TexCoords);
                break;
            }
        }
    }

    return Result;
}

rndr::Vector4r rndr::Sampler2D::SampleNearestNeighbor(const Image* I, const Point2r& TexCoords)
{
    const real U = TexCoords.X;
    const real V = TexCoords.Y;

    const real X = (I->m_Width - 1) * U;
    const real Y = (I->m_Height - 1) * V;

    const rndr::Point2i NearestDesc{(int)X, (int)Y};
    return I->GetPixelColor(NearestDesc);
}

rndr::Vector4r rndr::Sampler2D::SampleBilinear(const Image* I, const Point2r& TexCoords)
{
    const real U = TexCoords.X;
    const real V = TexCoords.Y;

    const real X = (I->m_Width - 1) * U;
    const real Y = (I->m_Height - 1) * V;

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

    real t = LOD - (real)Floor;

    if (m_Image->m_Props.MipFilter == ImageFiltering::Point)
    {
        return t > 0.5 ? CeilSample : FloorSample;
    }
    else
    {
        return rndr::Lerp(t, FloorSample, CeilSample);
    }
}

rndr::Point2r rndr::Sampler2D::Wrap(const Point2r& TexCoords) const
{
    Point2r NewTexCoords;
    NewTexCoords.X = Wrap(TexCoords.X, m_Image->m_Props.WrapU);
    NewTexCoords.Y = Wrap(TexCoords.Y, m_Image->m_Props.WrapV);
    return NewTexCoords;
}

real rndr::Sampler2D::Wrap(real Value, ImageWrapping WrapMethod) const
{
    switch (WrapMethod)
    {
        case ImageWrapping::Clamp:
        {
            Value = Clamp(Value, 0, 1);
            break;
        }
        case ImageWrapping::Border:
        {
            const bool bIsInRange = Value >= 0 && Value <= 1;
            Value = bIsInRange ? Value : Infinity;
            break;
        }
        case ImageWrapping::Repeat:
        {
            int WholeValue = (int)Value;
            Value = Value - WholeValue;
            break;
        }
        case ImageWrapping::MirrorRepeat:
        {
            int WholeValue = (int)Value;
            if (WholeValue % 2 == 0)
            {
                Value = Value - WholeValue;
            }
            else
            {
                Value = Value - WholeValue;
                Value = 1 - Value;
            }
            break;
        }
        default:
        {
            assert(false);
        }
    }

    return Value;
}

#endif  // RNDR_RASTER
