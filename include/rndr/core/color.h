#pragma once

#include "rndr/core/rndr.h"

namespace rndr
{

struct Color
{
    real R, G, B, A;

    /**
     * Constructor.
     *
     * @param Value Color value in uint32_t.
     * @param Layout the position and size of channels in a pixel value.
     */
    Color(uint32_t Value, PixelLayout Layout = PixelLayout::A8R8G8B8);

    Color(real RR, real GG, real BB, real AA) : R(RR), G(GG), B(BB), A(AA) {}

    Color();

    /**
     * Convert Color to specified PixelLayout.
     *
     * @return Returns uint32_t value that represents color.
     */
    uint32_t ToUInt(rndr::PixelLayout Layout = rndr::PixelLayout::A8R8G8B8) const;

    /**
     * Apply gamma correction to the color that is in linear space.
     *
     * @return Color in gamma correction space (color ^ (1 / gamma)).
     *
     * @note Using gamma of 2 for easier calculation.
     */
    Color ToGammaCorrectSpace(real Gamma) const;

    /**
     * Convert gamma corrected color back to linear space.
     *
     * @return Color in linear space (gamma_corrected_color ^ (gamma)).
     *
     * @note Using gamma of 2 for easier calculation.
     */
    Color ToLinearSpace(real Gamma) const;

    Color operator*(real Value) const;
    Color& operator*=(real Value);
    Color operator+(Color Other) const;
    Color& operator+=(Color Other);

public:
    static const rndr::Color Black;
    static const rndr::Color White;
    static const rndr::Color Red;
    static const rndr::Color Green;
    static const rndr::Color Blue;
    static const rndr::Color Pink;
};

}  // namespace rndr