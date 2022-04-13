#pragma once

#include "rndr/core/base.h"

// Contains all types used to configure a render pipeline.

namespace rndr
{

/**
 * Exact positions of channels and their size in bits.
 */
enum class PixelLayout
{
    A8R8G8B8,
    B8G8R8A8,
    R8G8B8A8,
    DEPTH_F32
};

enum class GammaSpace
{
    GammaCorrected,
    Linear
};

enum class ImageFileFormat
{
    BMP = 0,
    PNG,
    JPEG,
    NotSupported
};

enum class ImageFiltering
{
    NearestNeighbor,
    BilinearInterpolation,
    TrilinearInterpolation  // Valid only for minification filter
};

enum class DepthTest
{
    Never,
    Always,
    Less,
    Greater,
    Equal,
    NotEqual,
    LessEqual,
    GreaterEqual
};

enum class WindingOrder : int
{
    CW = -1,
    CCW = 1
};

enum class BlendFactor
{
    Zero,
    One,
    SrcColor,
    DstColor,
    OneMinusSrcColor,
    OneMinusDstColor,
    SrcAlpha,
    DstAlpha,
    OneMinusSrcAlpha,
    OneMinusDstAlpha,
    ConstColor,
    OneMinusConstColor,
    ConstAlpha,
    OneMinusConstAlpha
};

enum class BlendOperator
{
    Add,
    Subtract,         // Source - Destination
    ReverseSubtract,  // Destination - Source
    Min,
    Max
};

}  // namespace rndr