#include "rndr/core/color.h"

#include "rndr/core/log.h"
#include "rndr/core/math.h"

const rndr::Color rndr::Color::Black{0, 0, 0, 1, rndr::GammaSpace::GammaCorrected, true};
const rndr::Color rndr::Color::White{1, 1, 1, 1, rndr::GammaSpace::GammaCorrected, true};
const rndr::Color rndr::Color::Red{1, 0, 0, 1, rndr::GammaSpace::GammaCorrected, true};
const rndr::Color rndr::Color::Green{0, 1, 0, 1, rndr::GammaSpace::GammaCorrected, true};
const rndr::Color rndr::Color::Blue{0, 0, 1, 1, rndr::GammaSpace::GammaCorrected, true};
const rndr::Color rndr::Color::Pink{
    1, 0x69 / 255.0, 0xB4 / 255.0, 1, rndr::GammaSpace::GammaCorrected, true};

rndr::Color::Color()
{
    *this = Pink;
}

rndr::Color::Color(uint32_t Value,
                   rndr::GammaSpace Space,
                   PixelLayout Layout,
                   bool bIsPremultiplied)
{
    GammaSpace = Space;

    switch (Layout)
    {
        case PixelLayout::A8R8G8B8:
        {
            R = ((Value >> 16) & 0xFF) / 255.;
            G = ((Value >> 8) & 0xFF) / 255.;
            B = ((Value >> 0) & 0xFF) / 255.;
            A = ((Value >> 24) & 0xFF) / 255.;
            break;
        }
        case PixelLayout::B8G8R8A8:
        {
            R = ((Value >> 8) & 0xFF) / 255.;
            G = ((Value >> 16) & 0xFF) / 255.;
            B = ((Value >> 24) & 0xFF) / 255.;
            A = ((Value >> 0) & 0xFF) / 255.;
            break;
        }
        case PixelLayout::R8G8B8A8:
        {
            R = ((Value >> 24) & 0xFF) / 255.;
            G = ((Value >> 16) & 0xFF) / 255.;
            B = ((Value >> 8) & 0xFF) / 255.;
            A = ((Value >> 0) & 0xFF) / 255.;
            break;
        }
        default:
        {
            RNDR_LOG_ERROR("Color::Color: Unsupported pixel layout! Got %u", (uint32_t)Layout);
            assert(false);
            return;
        }
    }

    if (!bIsPremultiplied && A != 1)
    {
        R *= A;
        B *= A;
        G *= A;
    }
}

rndr::Color::Color(real RR,
                   real GG,
                   real BB,
                   real AA,
                   rndr::GammaSpace Space,
                   bool bIsPremultiplied)
    : R(RR), B(BB), G(GG), A(AA), GammaSpace(Space)
{
    if (!bIsPremultiplied && A != 1)
    {
        if (Space == rndr::GammaSpace::GammaCorrected)
        {
            R = rndr::ToGammaCorrectSpace(R);
            G = rndr::ToGammaCorrectSpace(G);
            B = rndr::ToGammaCorrectSpace(B);
        }

        R *= A;
        B *= A;
        G *= A;

        if (Space == rndr::GammaSpace::GammaCorrected)
        {
            R = rndr::ToLinearSpace(R);
            G = rndr::ToLinearSpace(G);
            B = rndr::ToLinearSpace(B);
        }
    }
}

uint32_t rndr::Color::ToUInt32(rndr::GammaSpace DesiredSpace, rndr::PixelLayout Layout) const
{
    Color ToTransform;

    if (GammaSpace == DesiredSpace)
    {
        ToTransform = *this;
    }
    else if (DesiredSpace == rndr::GammaSpace::GammaCorrected)
    {
        ToTransform = ToGammaCorrectSpace();
    }
    else
    {
        ToTransform = ToLinearSpace();
    }

    uint32_t RR = (uint32_t)std::roundf(ToTransform.R * 255);
    uint32_t GG = (uint32_t)std::roundf(ToTransform.G * 255);
    uint32_t BB = (uint32_t)std::roundf(ToTransform.B * 255);
    uint32_t AA = (uint32_t)std::roundf(ToTransform.A * 255);

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

rndr::Color rndr::Color::ToGammaCorrectSpace(real Gamma) const
{
    if (GammaSpace == rndr::GammaSpace::GammaCorrected)
    {
        return *this;
    }

    const bool IsPremultiplied = true;
    return Color(rndr::ToGammaCorrectSpace(R, Gamma), rndr::ToGammaCorrectSpace(G, Gamma),
                 rndr::ToGammaCorrectSpace(B, Gamma), A, rndr::GammaSpace::GammaCorrected,
                 IsPremultiplied);
}

rndr::Color rndr::Color::ToLinearSpace(real Gamma) const
{
    if (GammaSpace == rndr::GammaSpace::Linear)
    {
        return *this;
    }

    const bool IsPremultiplied = true;
    return Color(rndr::ToLinearSpace(R, Gamma), rndr::ToLinearSpace(G, Gamma),
                 rndr::ToLinearSpace(B, Gamma), A, rndr::GammaSpace::Linear, IsPremultiplied);
}

rndr::Color rndr::Color::operator*(real Value) const
{
    assert(GammaSpace == rndr::GammaSpace::Linear);

    const bool bIsPremultiplied = true;
    real RR = R * Value;
    real GG = G * Value;
    real BB = B * Value;
    RR = RR > 1 ? 1 : RR;
    GG = GG > 1 ? 1 : GG;
    BB = BB > 1 ? 1 : BB;

    return Color{RR, GG, BB, A, GammaSpace, bIsPremultiplied};
}

rndr::Color rndr::operator*(real Value, Color C)
{
    return C * Value;
}

rndr::Color& rndr::Color::operator*=(real Value)
{
    assert(GammaSpace == rndr::GammaSpace::Linear);

    R *= Value;
    G *= Value;
    B *= Value;

    R = R > 1 ? 1 : R;
    G = G > 1 ? 1 : G;
    B = B > 1 ? 1 : B;

    return *this;
}

rndr::Color rndr::Color::operator+(Color Other) const
{
    assert(GammaSpace == rndr::GammaSpace::Linear);
    assert(Other.GammaSpace == rndr::GammaSpace::Linear);

    const bool bIsPremultiplied = true;
    return Color(R + Other.R, G + Other.G, B + Other.B, std::min(A + Other.A, (real)1), GammaSpace,
                 bIsPremultiplied);
}

rndr::Color& rndr::Color::operator+=(Color Other)
{
    assert(GammaSpace == rndr::GammaSpace::Linear);
    assert(Other.GammaSpace == rndr::GammaSpace::Linear);

    R += Other.R;
    G += Other.G;
    B += Other.B;
    A = std::min(A + Other.A, (real)1);

    return *this;
}

rndr::Color rndr::Color::Blend(Color Src, Color Dst)
{
    Src = Src.ToLinearSpace();
    Dst = Dst.ToLinearSpace();

    const real InvSrcA = 1 - Src.A;

    return Src + Dst * InvSrcA;
}

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
        default:
        {
            assert(false);
        }
    }

    return 0;
}
