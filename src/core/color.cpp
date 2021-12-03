#include "rndr/core/color.h"

#include "rndr/core/math.h"

const rndr::Color rndr::Color::Black{0, 0, 0, 1};
const rndr::Color rndr::Color::White{1, 1, 1, 1};
const rndr::Color rndr::Color::Red{1, 0, 0, 1};
const rndr::Color rndr::Color::Green{0, 1, 0, 1};
const rndr::Color rndr::Color::Blue{0, 0, 1, 1};

rndr::Color::Color()
{
    *this = Black;
}

uint32_t rndr::Color::ToUInt() const
{
    uint32_t RR = (uint32_t)std::roundf(R * 255);
    uint32_t GG = (uint32_t)std::roundf(G * 255);
    uint32_t BB = (uint32_t)std::roundf(B * 255);
    uint32_t AA = (uint32_t)std::roundf(A * 255);

    uint32_t Result = 0;
    Result |= (RR << 16);
    Result |= (GG << 8);
    Result |= (BB << 0);
    Result |= (AA << 24);

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
