#include "rndr/core/pipeline.h"

real rndr::ToGammaCorrectSpace(real Value, real Gamma)
{
    if (Value <= 0.0031308)
    {
        return Value * 12.92;
    }

#if !defined(RNDR_REAL_AS_DOUBLE)
    return 1.055 * std::powf(Value, 1 / Gamma) - 0.055;
#else
    return 1.055 * std::pow(Value, 1 / Gamma) - 0.055;
#endif
}

real rndr::ToLinearSpace(real Value, real Gamma)
{
    if (Value <= 0.04045)
    {
        return Value / 12.92;
    }

    real Tmp = (Value + 0.055) / 1.055;

#if !defined(RNDR_REAL_AS_DOUBLE)
    return std::powf(Tmp, Gamma);
#else
    return std::pow(Tmp, Gamma);
#endif
}

rndr::Vector4r rndr::ToGammaCorrectSpace(const Vector4r& Color, real Gamma)
{
    return Vector4r(ToGammaCorrectSpace(Color.R, Gamma), ToGammaCorrectSpace(Color.G, Gamma), ToGammaCorrectSpace(Color.B, Gamma), Color.A);
}

rndr::Vector4r rndr::ToLinearSpace(const Vector4r& Color, real Gamma)
{
    return Vector4r(ToLinearSpace(Color.R, Gamma), ToLinearSpace(Color.G, Gamma), ToLinearSpace(Color.B, Gamma), Color.A);
}

rndr::Vector4r rndr::ToDesiredSpace(const Vector4r& Color, GammaSpace DesiredSpace, real Gamma)
{
    Vector4r Result;

    if (DesiredSpace == GammaSpace::GammaCorrected)
    {
        Result = ToGammaCorrectSpace(Color, Gamma);
    }
    else if (DesiredSpace == GammaSpace::Linear)
    {
        Result = ToLinearSpace(Color, Gamma);
    }
    else
    {
        assert(false);
    }

    return Result;
}

uint32_t rndr::ColorToUInt32(const Vector4r& Color, rndr::PixelLayout Layout)
{
    uint32_t RR = (uint32_t)std::roundf(Color.R * 255);
    uint32_t GG = (uint32_t)std::roundf(Color.G * 255);
    uint32_t BB = (uint32_t)std::roundf(Color.B * 255);
    uint32_t AA = (uint32_t)std::roundf(Color.A * 255);

    uint32_t Result = 0;

    switch (Layout)
    {
        case PixelLayout::A8R8G8B8:
        {
            Result |= (AA << 24);
            Result |= (RR << 16);
            Result |= (GG << 8);
            Result |= (BB << 0);
            break;
        }
        case PixelLayout::B8G8R8A8:
        {
            Result |= (BB << 24);
            Result |= (GG << 16);
            Result |= (RR << 8);
            Result |= (AA << 0);
            break;
        }
        case PixelLayout::R8G8B8A8:
        {
            Result |= (RR << 24);
            Result |= (GG << 16);
            Result |= (BB << 8);
            Result |= (AA << 0);
            break;
        }
        default:
        {
            assert(false);
        }
    }

    return Result;
}

rndr::Vector4r rndr::ColorToVector(uint32_t Color, PixelLayout Layout)
{
    Vector4r Vec;

    switch (Layout)
    {
        case PixelLayout::A8R8G8B8:
        {
            Vec.R = ((Color >> 16) & 0xFF) / 255.;
            Vec.G = ((Color >> 8) & 0xFF) / 255.;
            Vec.B = ((Color >> 0) & 0xFF) / 255.;
            Vec.A = ((Color >> 24) & 0xFF) / 255.;
            break;
        }
        case PixelLayout::B8G8R8A8:
        {
            Vec.R = ((Color >> 8) & 0xFF) / 255.;
            Vec.G = ((Color >> 16) & 0xFF) / 255.;
            Vec.B = ((Color >> 24) & 0xFF) / 255.;
            Vec.A = ((Color >> 0) & 0xFF) / 255.;
            break;
        }
        case PixelLayout::R8G8B8A8:
        {
            Vec.R = ((Color >> 24) & 0xFF) / 255.;
            Vec.G = ((Color >> 16) & 0xFF) / 255.;
            Vec.B = ((Color >> 8) & 0xFF) / 255.;
            Vec.A = ((Color >> 0) & 0xFF) / 255.;
            break;
        }
        default:
        {
            assert(false);
        }
    }

    return Vec;
}

int rndr::GetPixelSize(PixelLayout Layout)
{
    switch (Layout)
    {
        case PixelLayout::A8R8G8B8:
        case PixelLayout::B8G8R8A8:
        case PixelLayout::R8G8B8A8:
        {
            return 4;
        }
        case PixelLayout::DEPTH_F32:
        {
            return sizeof(real);
        }
        case PixelLayout::STENCIL_UINT8:
        {
            return sizeof(uint8_t);
        }
        default:
        {
            assert(false);
        }
    }

    return 0;
}
