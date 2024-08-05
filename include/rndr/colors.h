#pragma once

#include "rndr/math.h"

namespace Rndr
{

/**
 * Some useful colors in linear space.
 */
struct Colors
{
    static constexpr Vector4f k_black = {0, 0, 0, 1};
    static constexpr Vector4f k_white = {1, 1, 1, 1};
    static constexpr Vector4f k_red = {1, 0, 0, 1};
    static constexpr Vector4f k_green = {0, 1, 0, 1};
    static constexpr Vector4f k_blue = {0, 0, 1, 1};
    static constexpr Vector4f k_pink = {1, 0x69 / 255.0f, 0xB4 / 255.0f, 1};
    static constexpr Vector4f k_yellow = {1, 1, 0, 1};
};

}  // namespace Rndr
