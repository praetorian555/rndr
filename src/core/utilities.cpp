#include "rndr/core/utilities.h"

#include <cmath>
#include <filesystem>

#include "rndr/render/image.h"

// Private stuff
#include "utilities/bmpparser.h"

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
