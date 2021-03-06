#pragma once

#include "math/vector3.h"
#include "math/vector4.h"

#include "rndr/core/base.h"
#include "rndr/core/colors.h"
#include "rndr/core/span.h"

namespace rndr
{

struct GraphicsConstants
{
    static constexpr int MaxFrameBufferColorBuffers = 4;
    static constexpr int MaxInputLayoutEntries = 32;
    static constexpr int MaxShaderResourceBindSlots = 128;
    static constexpr int MaxConstantBuffers = 16;
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
    DEPTH24_STENCIL8,
    R32G32B32A32_FLOAT,
    R32G32B32_FLOAT,
    R32G32_FLOAT,
    R32_FLOAT
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
    MinMagMipPoint = 0,
    MinMagMipLinear,
    MinMagPoint_MipLinear,
    MinPoint_MagLinear_MipPoint,
    MinPoint_MagMipLinear,
    MinLinear_MagMipPoint,
    MinLinear_MagPoint_MipLinear,
    MinMagLinear_MipPoint,
    Anisotropic,
};

enum class ImageAddressing
{
    Repeat,
    MirrorRepeat,
    Clamp,
    Border,
    MirrorOnce
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
    None = 0,
    Front,
    Back,

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
enum class Comparator
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

enum class BufferBindFlag
{
    Vertex,
    Constant,
    Index
};

namespace ImageBindFlags
{
enum : uint32_t
{
    RenderTarget = 1 << 0,
    DepthStencil = 1 << 1,
    ShaderResource = 1 << 2
};
}

struct BufferProperties
{
    BufferBindFlag BindFlag;
    CPUAccess CPUAccess;
    Usage Usage;
    int Size;
    int Stride;
};

struct ImageProperties
{
    PixelFormat PixelFormat = PixelFormat::R8G8B8A8_UNORM_SRGB;
    CPUAccess CPUAccess = CPUAccess::None;
    Usage Usage = Usage::GPURead;
    bool bUseMips = false;
    uint32_t ImageBindFlags = ImageBindFlags::RenderTarget | ImageBindFlags::ShaderResource;
    int ArraySize = 1;
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
    ImageAddressing AddressingU = ImageAddressing::Repeat;
    ImageAddressing AddressingV = ImageAddressing::Repeat;
    ImageAddressing AddressingW = ImageAddressing::Repeat;

    math::Vector4 WrapBorderColor = Colors::Pink;

    ImageFiltering Filter = ImageFiltering::MinMagMipLinear;

    real LODBias = 0;
    real MinLOD = 0;
    real MaxLOD = 50'000;

    uint32_t MaxAnisotropy = 0;
};

struct ConstantBufferProperties
{
    int Size;
    Usage Usage = Usage::GPUReadCPUWrite;
    CPUAccess CPUAccess = CPUAccess::Write;
};

enum class DataRepetition
{
    PerVertex,
    PerInstance
};

enum class ShaderType
{
    Vertex,
    Fragment
};

struct ShaderProperties
{
    ShaderType Type;

    std::wstring FilePath;
    std::string EntryPoint;
    bool bCompilationNeeded = true;
};

enum class DepthMask
{
    None,
    All
};

enum class FillMode
{
    Solid,
    Wireframe
};

constexpr int AppendAlignedElement = -1;
struct InputLayoutProperties
{
    std::string SemanticName;
    int SemanticIndex;
    PixelFormat Format;
    int OffsetInVertex;  // Use AppendAlignedElement to align it to the previous element
    // Corresponds to the index of a vertex buffer in a mesh
    int InputSlot;
    DataRepetition Repetition;
    // In case data is repeated on instance level this allows us to skip some instances
    int InstanceStepRate;
};

struct RasterizerProperties
{
    FillMode FillMode = FillMode::Solid;
    WindingOrder FrontFaceWindingOrder = WindingOrder::CCW;
    Face CullFace = Face::Back;
    int DepthBias = 0;
    real DepthBiasClamp = 0.0;
    real SlopeScaledDepthBias = 0.0;
    bool bDepthClipEnable;
    bool bScissorEnable;
    bool bMultisampleEnable;
    bool bAntialiasedLineEnable;
};

struct DepthStencilProperties
{
    bool bDepthEnable = false;
    Comparator DepthComparator = Comparator::Less;
    DepthMask DepthMask = DepthMask::All;

    bool bStencilEnable = false;
    uint8_t StencilReadMask = 0xFF;
    uint8_t StencilWriteMask = 0xFF;
    uint32_t StencilRefValue = 0;

    StencilOperation StencilFrontFaceFailOp = StencilOperation::Keep;
    StencilOperation StencilFrontFaceDepthFailOp = StencilOperation::Keep;
    StencilOperation StencilFrontFacePassOp = StencilOperation::Keep;
    Comparator StencilFrontFaceComparator = Comparator::Never;

    StencilOperation StencilBackFaceFailOp = StencilOperation::Keep;
    StencilOperation StencilBackFaceDepthFailOp = StencilOperation::Keep;
    StencilOperation StencilBackFacePassOp = StencilOperation::Keep;
    Comparator StencilBackFaceComparator = Comparator::Never;
};

struct BlendProperties
{
    bool bBlendEnable = true;
    BlendFactor SrcColorFactor;
    BlendFactor DstColorFactor;
    BlendOperator ColorOperator;
    BlendFactor SrcAlphaFactor;
    BlendFactor DstAlphaFactor;
    BlendOperator AlphaOperator;
    math::Vector3 ConstColor;
    real ConstAlpha;
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
math::Vector4 ToGammaCorrectSpace(const math::Vector4& Color, real Gamma = RNDR_GAMMA);

/**
 * Convert gamma corrected (sRGB) color back to linear space.
 * @param Color Value to be converted to linear space.
 * @return Returns color value in linear space (gamma_corrected_color ^ (gamma)).
 * @note Using gamma of 2 for easier calculation.
 */
math::Vector4 ToLinearSpace(const math::Vector4& Color, real Gamma = RNDR_GAMMA);

/**
 * Convert Color to the desired gamma space.
 * @param Color Value to convert.
 * @param DesiredSpace Gamma space to convert to.
 * @param Gamma Gamma value to use for conversion.
 * @return Returns color in vector form in desired gamma space.
 */
math::Vector4 ToDesiredSpace(const math::Vector4& Color, GammaSpace DesiredSpace, real Gamma = RNDR_GAMMA);

/**
 * Convert a color from a vector representation to packed pixel representation.
 * @param Color Color to convert.
 * @param Layout Organization and size of color channels in a packed pixel representation.
 * @return Returns packed pixel representation.
 */
uint32_t ColorToUInt32(const math::Vector4& Color, PixelFormat Format);

/**
 * Convert color from a packed pixel representation to a vector representation.
 * @param Color Color to convert.
 * @param Layout Organization and size of color channels in a packed pixel representation.
 * @return Returns vector representation of a color.
 */
math::Vector4 ColorToVector(uint32_t Color, PixelFormat Format);

/**
 * Get size of a pixel in bytes based on the layout.
 * @param Layout Specifies the order of channels and their size.
 * @return Returns the size of a pixel in bytes.
 */
int GetPixelSize(PixelFormat Format);

}  // namespace rndr
