#include "rndr/core/graphicstypes.h"

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

math::Vector4 rndr::ToGammaCorrectSpace(const math::Vector4& Color, real Gamma)
{
    return math::Vector4(ToGammaCorrectSpace(Color.R, Gamma), ToGammaCorrectSpace(Color.G, Gamma), ToGammaCorrectSpace(Color.B, Gamma),
                         Color.A);
}

math::Vector4 rndr::ToLinearSpace(const math::Vector4& Color, real Gamma)
{
    return math::Vector4(ToLinearSpace(Color.R, Gamma), ToLinearSpace(Color.G, Gamma), ToLinearSpace(Color.B, Gamma), Color.A);
}

math::Vector4 rndr::ToDesiredSpace(const math::Vector4& Color, GammaSpace DesiredSpace, real Gamma)
{
    math::Vector4 Result;

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

uint32_t rndr::ColorToUInt32(const math::Vector4& Color, PixelFormat Format)
{
    uint32_t RR = (uint32_t)std::roundf(Color.R * 255);
    uint32_t GG = (uint32_t)std::roundf(Color.G * 255);
    uint32_t BB = (uint32_t)std::roundf(Color.B * 255);
    uint32_t AA = (uint32_t)std::roundf(Color.A * 255);

    uint32_t Result = 0;

    switch (Format)
    {
        case PixelFormat::R8G8B8A8_UNORM:
        case PixelFormat::R8G8B8A8_UNORM_SRGB:
        {
            Result |= (RR << 24);
            Result |= (GG << 16);
            Result |= (BB << 8);
            Result |= (AA << 0);
            break;
        }
        case PixelFormat::B8G8R8A8_UNORM:
        case PixelFormat::B8G8R8A8_UNORM_SRGB:
        {
            Result |= (BB << 24);
            Result |= (GG << 16);
            Result |= (RR << 8);
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

math::Vector4 rndr::ColorToVector(uint32_t Color, PixelFormat Format)
{
    math::Vector4 Vec;

    switch (Format)
    {
        case PixelFormat::R8G8B8A8_UNORM:
        case PixelFormat::R8G8B8A8_UNORM_SRGB:
        {
            Vec.R = ((Color >> 24) & 0xFF) / 255.;
            Vec.G = ((Color >> 16) & 0xFF) / 255.;
            Vec.B = ((Color >> 8) & 0xFF) / 255.;
            Vec.A = ((Color >> 0) & 0xFF) / 255.;
            break;
        }
        case PixelFormat::B8G8R8A8_UNORM:
        case PixelFormat::B8G8R8A8_UNORM_SRGB:
        {
            Vec.R = ((Color >> 8) & 0xFF) / 255.;
            Vec.G = ((Color >> 16) & 0xFF) / 255.;
            Vec.B = ((Color >> 24) & 0xFF) / 255.;
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

int rndr::GetPixelSize(PixelFormat Format)
{
    switch (Format)
    {
        case PixelFormat::R8G8B8A8_UNORM:
        case PixelFormat::R8G8B8A8_UNORM_SRGB:
        case PixelFormat::B8G8R8A8_UNORM:
        case PixelFormat::B8G8R8A8_UNORM_SRGB:
        case PixelFormat::DEPTH24_STENCIL8:
        {
            return 4;
        }
        default:
        {
            assert(false);
        }
    }

    return 4;
}
