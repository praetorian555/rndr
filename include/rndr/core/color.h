#pragma once

#include "rndr/core/rndr.h"

namespace rndr
{

/**
 * Represents color in a form that is easily used in computations.
 *
 * Internally it will always store values premultiplied by the alpha.
 */
struct Color
{
    real R, G, B, A;

    rndr::GammaSpace GammaSpace = rndr::GammaSpace::Linear;

    Color();
    Color(uint32_t Value, rndr::GammaSpace Space, PixelLayout Layout, bool bIsPremultiplied);
    Color(real RR, real GG, real BB, real AA, rndr::GammaSpace Space, bool bIsPremultiplied);

    /**
     * Convert Color to specified PixelLayout.
     *
     * @return Returns uint32_t value that represents color.
     */
    uint32_t ToUInt32(rndr::GammaSpace DesiredSpace, rndr::PixelLayout Layout) const;

    /**
     * Apply gamma correction to the color that is in linear space.
     *
     * @return Color in gamma correction space (color ^ (1 / gamma)).
     *
     * @note Using gamma of 2 for easier calculation.
     */
    Color ToGammaCorrectSpace(real Gamma = RNDR_GAMMA) const;

    /**
     * Convert gamma corrected color back to linear space.
     *
     * @return Color in linear space (gamma_corrected_color ^ (gamma)).
     *
     * @note Using gamma of 2 for easier calculation.
     */
    Color ToLinearSpace(real Gamma = RNDR_GAMMA) const;

    Color operator*(real Value) const;
    Color& operator*=(real Value);
    Color operator+(Color Other) const;
    Color& operator+=(Color Other);

    /**
     * Blend Dst color into Src color using over operator. Result is a color in linear gamma space.
     *
     * TODO(mkostic): Add support for other operators.
     */
    static Color Blend(Color Src, Color Dst);

public:
    static const rndr::Color Black;
    static const rndr::Color White;
    static const rndr::Color Red;
    static const rndr::Color Green;
    static const rndr::Color Blue;
    static const rndr::Color Pink;
};

}  // namespace rndr