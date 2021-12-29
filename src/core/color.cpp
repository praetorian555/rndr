#include "rndr/core/color.h"

#include "rndr/core/math.h"
#include "rndr/core/utilities.h"

const rndr::Color rndr::Color::Black{0, 0, 0, 1};
const rndr::Color rndr::Color::White{1, 1, 1, 1};
const rndr::Color rndr::Color::Red{1, 0, 0, 1};
const rndr::Color rndr::Color::Green{0, 1, 0, 1};
const rndr::Color rndr::Color::Blue{0, 0, 1, 1};
const rndr::Color rndr::Color::Pink{1, 0x69 / 255.0, 0xB4 / 255.0, 1};

rndr::Color::Color()
{
    *this = Pink;
}

rndr::Color::Color(uint32_t Value, PixelLayout Layout)
{
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
        default:
        {
            assert(false);
        }
    }
}

uint32_t rndr::Color::ToUInt(rndr::PixelLayout Layout) const
{
    uint32_t RR = (uint32_t)std::roundf(R * 255);
    uint32_t GG = (uint32_t)std::roundf(G * 255);
    uint32_t BB = (uint32_t)std::roundf(B * 255);
    uint32_t AA = (uint32_t)std::roundf(A * 255);

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
        default:
        {
            assert(false);
        }
    }

    return Result;
}

rndr::Color rndr::Color::ToGammaCorrectSpace(real Gamma) const
{
    return Color(rndr::ToGammaCorrectSpace(R, Gamma), rndr::ToGammaCorrectSpace(G, Gamma),
                 rndr::ToGammaCorrectSpace(B, Gamma), A);
}

rndr::Color rndr::Color::ToLinearSpace(real Gamma) const
{
    return Color(rndr::ToLinearSpace(R, Gamma), rndr::ToLinearSpace(G, Gamma),
                 rndr::ToLinearSpace(B, Gamma), A);
}

rndr::Color rndr::Color::operator*(real Value) const
{
    return Color{R * Value, G * Value, B * Value, A * Value};
}

rndr::Color& rndr::Color::operator*=(real Value)
{
    R *= Value;
    G *= Value;
    B *= Value;
    A *= Value;

    return *this;
}

rndr::Color rndr::Color::operator+(Color Other) const
{
    return Color(R + Other.R, G + Other.G, B + Other.B, std::min(A + Other.A, (real)1));
}

rndr::Color& rndr::Color::operator+=(Color Other)
{
    R += Other.R;
    G += Other.G;
    B += Other.B;
    A = std::min(A + Other.A, (real)1);

    return *this;
}
