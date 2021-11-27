#include "rndr/color.h"

#include <cmath>

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

rndr::Color rndr::Color::ToGammaCorrectSpace() const
{
    return Color(std::sqrt(R), std::sqrt(G), std::sqrt(B), A);
}

rndr::Color rndr::Color::ToLinearSpace() const
{
    return Color(R * R, G * G, B * B, A);
}