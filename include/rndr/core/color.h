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
     * @param Value Color in AARRGGBB format.
     */
    Color(uint32_t Value)
    {
        R = (Value >> 16) & 0xFF;
        G = (Value >> 8) & 0xFF;
        B = (Value >> 0) & 0xFF;
        A = (Value >> 24) & 0xFF;
    }

    Color(real RR, real GG, real BB, real AA) : R(RR), G(GG), B(BB), A(AA) {}

    Color();

    /**
     * Convert Color to AARRGGBB form.
     *
     * @return Returns uint32_t value that represents color.
     */
    uint32_t ToUInt() const;

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
};

}  // namespace rndr