#pragma once

#include "rndr/rndr.h"

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
    Color ToGammaCorrectSpace() const;

    /**
     * Convert gamma corrected color back to linear space.
     *
     * @return Color in linear space (gamma_corrected_color ^ (gamma)).
     *
     * @note Using gamma of 2 for easier calculation.
     */
    Color ToLinearSpace() const;
};

}  // namespace rndr