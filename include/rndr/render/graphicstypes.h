#pragma once

#include <string>

#include "math/vector3.h"
#include "math/vector4.h"

#include "rndr/core/base.h"
#include "rndr/core/span.h"

#include "rndr/render/colors.h"

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
 *
 * UNORM - Interpreted by a shader as floating-point value in the range [0, 1].
 * SNORM - Interpreted by a shader as floating-point value in the range [-1, 1].
 */
enum class PixelFormat
{
    R8G8B8A8_UNORM = 0,
    R8G8B8A8_UNORM_SRGB,
    R8G8B8A8_UINT,
    R8G8B8A8_SNORM,
    R8G8B8A8_SINT,
    B8G8R8A8_UNORM,
    B8G8R8A8_UNORM_SRGB,

    D24_UNORM_S8_UINT,

    R8_UNORM,
    R8_UINT,
    R8_SNORM,
    R8_SINT,

    R32G32B32A32_FLOAT,
    R32G32B32A32_UINT,
    R32G32B32A32_SINT,

    R32G32B32_FLOAT,
    R32G32B32_UINT,
    R32G32B32_SINT,

    R32G32_FLOAT,
    R32G32_UINT,
    R32G32_SINT,

    R32_FLOAT,
    R32_UINT,
    R32_SINT,

    R32_TYPELESS,
    R16_TYPELESS,

    Count
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

enum class Usage
{
    // Use this when you need to update the resource from the CPU side sparingly and mostly it is
    // only processed or used by GPU
    Default,
    // Used when we need to update resource every frame, such as constant buffers
    Dynamic,
    // Used when we need to move data from the GPU to the CPU side, for example after rendering
    Readback
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
    InvSrcColor,
    InvDstColor,
    SrcAlpha,
    DstAlpha,
    InvSrcAlpha,
    InvDstAlpha,
    ConstColor,
    InvConstColor,
    ConstAlpha,
    InvConstAlpha
};

enum class BlendOperator
{
    Add,
    Subtract,         // Source - Destination
    ReverseSubtract,  // Destination - Source
    Min,
    Max
};

enum class BufferType
{
    Readback,
    Vertex,
    Index,
    Constant,
    UnorderedAccess
};

namespace ImageBindFlags
{
enum : uint32_t
{
    RenderTarget = 1 << 0,
    DepthStencil = 1 << 1,
    ShaderResource = 1 << 2,
    UnorderedAccess = 1 << 3,
};
}

enum class DataRepetition
{
    PerVertex,
    PerInstance
};

enum class ShaderType
{
    Vertex,
    Fragment,
    Compute
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

struct BufferProperties
{
    BufferType Type = BufferType::Constant;
    Usage Usage = Usage::Default;
    // Total size of a buffer
    uint32_t Size = 0;
    // Size of one element, in bytes, in an array of elements
    uint32_t Stride = 0;
};

struct ImageProperties
{
    PixelFormat PixelFormat = PixelFormat::R8G8B8A8_UNORM_SRGB;
    Usage Usage = Usage::Default;
    bool bUseMips = false;
    uint32_t ImageBindFlags = ImageBindFlags::ShaderResource;
    uint32_t SampleCount = 1;
};

struct FrameBufferProperties
{
    int ColorBufferCount = 1;
    ImageProperties ColorBufferProperties[GraphicsConstants::MaxFrameBufferColorBuffers];

    bool bUseDepthStencil = false;
    ImageProperties DepthStencilBufferProperties;
};

struct GraphicsContextProperties
{
    bool bEnableDebugLayer = true;
    bool bFailWarning = true;

    bool bMakeThreadSafe = true;
    // In case of workloads that last more then 2 seconds don't trigger a timeout
    bool bDisableGPUTimeout = false;
};

struct SwapChainProperties
{
    PixelFormat ColorFormat = PixelFormat::R8G8B8A8_UNORM;
    PixelFormat DepthStencilFormat = PixelFormat::D24_UNORM_S8_UINT;
    bool bUseDepthStencil = false;
    bool bWindowed = true;
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
    real MaxLOD = math::kLargestFloat;
    Comparator Comp = Comparator::Never;

    uint32_t MaxAnisotropy = 0;
};

struct ShaderMacro
{
    std::string Name;
    std::string Definition;
};

struct ShaderProperties
{
    ShaderType Type;

    std::string EntryPoint;
    std::vector<ShaderMacro> Macros;
    // TODO: Add support for include files
};

constexpr int AppendAlignedElement = -1;
struct InputLayoutProperties
{
    std::string SemanticName;
    int SemanticIndex = 0;
    PixelFormat Format = PixelFormat::R32G32B32_FLOAT;
    int OffsetInVertex = 0;  // Use AppendAlignedElement to align it to the previous element
    // Corresponds to the index of a vertex buffer in a mesh
    int InputSlot = 0;
    DataRepetition Repetition = DataRepetition::PerVertex;
    // In case data is repeated on instance level this allows us to skip some instances
    int InstanceStepRate = 0;
};

struct RasterizerProperties
{
    FillMode FillMode = FillMode::Solid;
    WindingOrder FrontFaceWindingOrder = WindingOrder::CCW;
    Face CullFace = Face::Back;
    int DepthBias = 0;
    real DepthBiasClamp = 0.0;
    real SlopeScaledDepthBias = 0.0;
    bool bDepthClipEnable = true;
    bool bScissorEnable = false;
    bool bMultisampleEnable = false;
    bool bAntialiasedLineEnable = false;
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
    BlendFactor SrcColorFactor = BlendFactor::SrcAlpha;
    BlendFactor DstColorFactor = BlendFactor::InvSrcAlpha;
    BlendOperator ColorOperator = BlendOperator::Add;
    BlendFactor SrcAlphaFactor = BlendFactor::One;
    BlendFactor DstAlphaFactor = BlendFactor::InvSrcAlpha;
    BlendOperator AlphaOperator = BlendOperator::Add;
    math::Vector3 ConstColor;
    real ConstAlpha = 0.0f;
};

struct PipelineProperties
{
    Span<InputLayoutProperties> InputLayout;

    ShaderProperties VertexShader;
    ByteSpan VertexShaderContents;
    ShaderProperties PixelShader;
    ByteSpan PixelShaderContents;

    RasterizerProperties Rasterizer;
    BlendProperties Blend;
    DepthStencilProperties DepthStencil;
};

// Helper functions

/**
 * Get size of a pixel in bytes based on the layout.
 * @param Layout Specifies the order of channels and their size.
 * @return Returns the size of a pixel in bytes.
 */
int GetPixelSize(PixelFormat Format);

}  // namespace rndr
