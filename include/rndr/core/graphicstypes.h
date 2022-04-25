#pragma once

#include "rndr/core/base.h"
#include "rndr/core/colors.h"
#include "rndr/core/math.h"

// Contains all types used to configure a render pipeline.

namespace rndr
{

struct GraphicsConstants
{
    static constexpr int MaxFrameBufferColorBuffers = 4;
};

/**
 * Exact positions of channels and their size in bits.
 */
enum class PixelFormat
{
    R8G8B8A8_UNORM,
    R8G8B8A8_UNORM_SRGB,
    B8G8R8A8_UNORM,
    B8G8R8A8_UNORM_SRGB,
    DEPTH24_STENCIL8
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
    Point,   // Sometimes called NearestNeighbour
    Linear,  // Also bilinear
};

enum class ImageWrapping
{
    Clamp,
    Border,
    Repeat,
    MirrorRepeat
};

enum class CPUAccess
{
    None,
    Read,  // Only if Usage is FromGPUToCPU
    Write  // Only if Usage is GPUReadCPUWrite or FromGPUToCPU
};

enum class Usage
{
    GPUReadWrite,  // Default
    GPURead,
    GPUReadCPUWrite,
    FromGPUToCPU  // Data transfer
};

enum class WindingOrder : uint32_t
{
    CW = 0,
    CCW
};

enum class Face : uint32_t
{
    Front = 0,
    Back,
    FrontBack,

    Count
};

enum class PrimitiveTopology
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip
};

// TODO(mkostic): Probably merge DepthTest and StencilComparator in a single enum
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

enum class StencilComparator
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

enum class StencilOperation
{
    Keep,
    Invert,
    Zero,
    Replace,
    Increment,
    IncrementWrap,
    Decrement,
    DecrementWrap
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

struct ImageProperties
{
    PixelFormat PixelFormat = PixelFormat::R8G8B8A8_UNORM_SRGB;
    CPUAccess CPUAccess = CPUAccess::None;
    Usage Usage = Usage::GPUReadWrite;
    bool bUseMips = false;
};

struct FrameBufferProperties
{
    bool bWindowFrameBuffer = false;

    int ColorBufferCount = 1;
    ImageProperties ColorBufferProperties[GraphicsConstants::MaxFrameBufferColorBuffers];

    bool bUseDepthStencil = false;
    ImageProperties DepthStencilBufferProperties;
};

struct GraphicsContextProperties
{
    int WindowWidth = 0;
    int WindowHeight = 0;

    FrameBufferProperties FrameBuffer;
};

struct SamplerProperties
{
    ImageWrapping WrapU = ImageWrapping::Repeat;
    ImageWrapping WrapV = ImageWrapping::Repeat;
    Vector4r WrapBorderColor = Colors::Pink;

    ImageFiltering MagFilter = ImageFiltering::Point;
    ImageFiltering MinFilter = ImageFiltering::Point;
    ImageFiltering MipFilter = ImageFiltering::Linear;

    int LODBias = 0;
};

// Helper functions

/**
 * Converts value to gamma correct space from linear space. Value^(1 / Gamma).
 * @param Value Value to convert. Must be in range [0, 1].
 * @param Gamma Exponent. Default value is RNDR_GAMMA.
 * @return Returns converted value in range [0, 1].
 */
real ToGammaCorrectSpace(real Value, real Gamma = RNDR_GAMMA);

/**
 * Converts value to linear space from a gamma correct space. Value^(Gamma).
 * @param Value Value to convert. Must be in range [0, 1].
 * @param Gamma Exponent. Default value is RNDR_GAMMA.
 * @return Returns converted value in range [0, 1].
 */
real ToLinearSpace(real Value, real Gamma = RNDR_GAMMA);

/**
 * Apply gamma correction to the color that is in linear space.
 * @param Color Value to be converted to gamma correct (sRGB) space.
 * @param Gamma Gamma value to be used. By default it uses RNDR_GAMMA.
 * @return Returns color in gamma correction space (color ^ (1 / gamma)).
 */
Vector4r ToGammaCorrectSpace(const Vector4r& Color, real Gamma = RNDR_GAMMA);

/**
 * Convert gamma corrected (sRGB) color back to linear space.
 * @param Color Value to be converted to linear space.
 * @return Returns color value in linear space (gamma_corrected_color ^ (gamma)).
 * @note Using gamma of 2 for easier calculation.
 */
Vector4r ToLinearSpace(const Vector4r& Color, real Gamma = RNDR_GAMMA);

/**
 * Convert Color to the desired gamma space.
 * @param Color Value to convert.
 * @param DesiredSpace Gamma space to convert to.
 * @param Gamma Gamma value to use for conversion.
 * @return Returns color in vector form in desired gamma space.
 */
Vector4r ToDesiredSpace(const Vector4r& Color, GammaSpace DesiredSpace, real Gamma = RNDR_GAMMA);

/**
 * Convert a color from a vector representation to packed pixel representation.
 * @param Color Color to convert.
 * @param Layout Organization and size of color channels in a packed pixel representation.
 * @return Returns packed pixel representation.
 */
uint32_t ColorToUInt32(const Vector4r& Color, PixelFormat Format);

/**
 * Convert color from a packed pixel representation to a vector representation.
 * @param Color Color to convert.
 * @param Layout Organization and size of color channels in a packed pixel representation.
 * @return Returns vector representation of a color.
 */
Vector4r ColorToVector(uint32_t Color, PixelFormat Format);

/**
 * Get size of a pixel in bytes based on the layout.
 * @param Layout Specifies the order of channels and their size.
 * @return Returns the size of a pixel in bytes.
 */
int GetPixelSize(PixelFormat Format);

}  // namespace rndr