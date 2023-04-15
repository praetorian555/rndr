#pragma once

#include "math/vector4.h"

#include "rndr/core/base.h"

namespace Rndr
{

/**
 * Some useful colors in linear space.
 */
struct Colors
{
    static constexpr math::Vector4 kBlack = {0, 0, 0, 1};
    static constexpr math::Vector4 kWhite = {1, 1, 1, 1};
    static constexpr math::Vector4 kRed = {1, 0, 0, 1};
    static constexpr math::Vector4 kGreen = {0, 1, 0, 1};
    static constexpr math::Vector4 kBlue = {0, 0, 1, 1};
    static constexpr math::Vector4 kPink = {1, 0x69 / 255.0f, 0xB4 / 255.0f, 1};
    static constexpr math::Vector4 kYellow = {1, 1, 0, 1};
};

}  // namespace rndr
