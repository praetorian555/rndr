#pragma once

#include "rndr/types.hpp"

namespace Rndr::Canvas
{

/**
 * Simplified data format enum covering both pixel formats and vertex attribute formats.
 * Canvas uses its own format vocabulary instead of exposing raw API-level formats.
 */
enum class Format : u8
{
    // Pixel formats.
    R8,
    RG8,
    RGB8,
    RGBA8,
    SRGB8,
    SRGBA8,
    R16F,
    RG16F,
    RGBA16F,
    R32F,
    RG32F,
    RGBA32F,
    D24S8,
    D32F,

    // Vertex data formats.
    Float1,
    Float2,
    Float3,
    Float4,
    Int1,
    Int2,
    Int3,
    Int4,

    EnumCount
};

}  // namespace Rndr::Canvas
