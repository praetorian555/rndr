#pragma once

#include "math/vector3.h"
#include "math/vector4.h"

#include "rndr/core/array.h"
#include "rndr/core/base.h"
#include "rndr/core/forward-def-windows.h"
#include "rndr/core/span.h"
#include "rndr/core/stack-array.h"
#include "rndr/core/string.h"
#include "rndr/render/colors.h"

namespace Rndr
{

class Shader;

struct GraphicsConstants
{
    static constexpr int kMaxFrameBufferColorBuffers = 4;
    static constexpr int kMaxInputLayoutEntries = 32;
    static constexpr int kMaxShaderResourceBindSlots = 128;
    static constexpr int kMaxConstantBuffers = 16;
};

enum class ImageType
{
    Image2D,
    Image2DArray,
    CubeMap
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

/**
 * Represents the filtering used to resolve the value of the image when it is sampled.
 */
enum class ImageFilter
{
    /** Returns the value of the texture element that is nearest to image coordinate. */
    Nearest = 0,

    /**
     * Returns the average of the four texture elements that are closest to the specified texture
     * coordinates.
     */
    Linear,

    /** Represents number of elements in the enum. */
    EnumCount
};

/**
 * Specifies how to handle image coordinate being outside the [0, 1] range.
 */
enum class ImageAddressMode
{
    /** Coordinate is clamped to the range [0, 1]. */
    Clamp = 0,

    /**
     * Similar to Clamp, but the coordinates that are clamped are resolved to the specified border
     * color.
     */
    Border,

    /** Integer part of the coordinate is ignored. */
    Repeat,

    /**
     * The coordinate is equal to the fractional part of the coordinate if the integer part is
     * even. If its odd the coordinate is equal to 1 minus fractional part of the coordinate.
     */
    MirrorRepeat,

    /**
     * Similar to MirrorRepeat, but after one repetition Clamp is used.
     */
    MirrorOnce,

    /** Represents number of elements in the enum. */
    EnumCount
};

/**
 * Represents different ways the GPU resource can be used.
 */
enum class Usage
{
    /**
     * Use this when you need to update the resource from the CPU side sparingly and mostly it is
     * only processed or used by GPU.
     */
    Default = 0,

    /** Used when we need to update resource every frame, such as constant buffers. */
    Dynamic,

    /** Used when we need to move data from the GPU to the CPU side, for example after rendering. */
    ReadBack,

    /** Represents number of elements in the enum. */
    EnumCount
};

/**
 * Represents the winding order of a triangle. This is used to determine the front face of a
 * triangle.
 */
enum class WindingOrder : uint32_t
{
    /** Clockwise winding order. */
    CW = 0,

    /** Counter-clockwise winding order. */
    CCW
};

/**
 * Represents the faces of a triangle that are visible to the camera.
 */
enum class Face : uint32_t
{
    /** Both front and back faces are visible. */
    None = 0,

    /** Only front faces are visible. */
    Front,

    /** Only back faces are visible. */
    Back
};

enum class PrimitiveTopology
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip
};

/**
 * Represents a comparator function used in depth and stencil tests. The function compares the
 * incoming value with the stored value and decides whether to pass the test.
 */
enum class Comparator
{
    /** Never passes the test. */
    Never = 0,

    /** Always passes the test. */
    Always,

    /** Passes the test if the incoming value is less than the stored value. */
    Less,

    /** Passes the test if the incoming value is greater than the stored value. */
    Greater,

    /** Passes the test if the incoming value is equal to the stored value. */
    Equal,

    /** Passes the test if the incoming value is not equal to the stored value. */
    NotEqual,

    /** Passes the test if the incoming value is less than or equal to the stored value. */
    LessEqual,

    /** Passes the test if the incoming value is greater than or equal to the stored value. */
    GreaterEqual,

    /** Represents the number of elements in the enum. */
    EnumCount
};

/**
 * Represents an operation that is performed on the stencil buffer after the stencil test completes.
 */
enum class StencilOperation
{
    /** Keeps the current value. */
    Keep = 0,

    /** Sets the stencil buffer value to 0. */
    Zero,

    /** Sets the stencil buffer value to the reference value. */
    Replace,

    /** Increments the current value and clamps it to the maximum representable unsigned value. */
    Increment,

    /**
     * Increments the current value and wraps it to 0 when incrementing the maximum representable
     * unsigned value.
     */
    IncrementWrap,

    /** Decrements the current value and clamps it to 0. */
    Decrement,

    /**
     * Decrements the current value and wraps it to the maximum representable unsigned value when
     * decrementing a value of 0.
     */
    DecrementWrap,

    /** Bitwise inverts the current value. */
    Invert,

    /** Represents the number of elements in the enum. */
    EnumCount
};

/**
 * Represents a blend factor that is used in blending operations. Note that inverse here means
 * "1 - x".
 */
enum class BlendFactor
{
    /** Factor is 0. */
    Zero = 0,

    /** Factor is 1. */
    One,

    /** Factor is the source color. */
    SrcColor,

    /** Factor is the destination color. */
    DstColor,

    /** Factor is the inverse of the source color. */
    InvSrcColor,

    /** Factor is the inverse of the destination color. */
    InvDstColor,

    /** Factor is the source alpha. */
    SrcAlpha,

    /** Factor is the destination alpha. */
    DstAlpha,

    /** Factor is the inverse of the source alpha. */
    InvSrcAlpha,

    /** Factor is the inverse of the destination alpha. */
    InvDstAlpha,

    /** Factor is the constant color specified in the BlendDesc. */
    ConstColor,

    /** Factor is the inverse of the constant color specified in the BlendDesc. */
    InvConstColor,

    /** Factor is the constant alpha specified in the BlendDesc. */
    ConstAlpha,

    /** Factor is the inverse of the constant alpha specified in the BlendDesc. */
    InvConstAlpha,

    /** Represents the number of valid entries in this enum. */
    EnumCount
};

/**
 * Represents a blend operation that is performed on a render target. The operation is performed
 * between the source and destination color.
 */
enum class BlendOperation
{
    /** Adds the source and destination colors. */
    Add = 0,

    /** Subtracts the destination color from the source color. */
    Subtract,

    /** Subtracts the source color from the destination color. */
    ReverseSubtract,

    /** Minimum of the source and destination colors. */
    Min,

    /** Maximum of the source and destination colors. */
    Max,

    /** Represents the number of valid entries in this enum. */
    EnumCount
};

/**
 * Represents what type of data is stored in a buffer. This is used to determine how the buffer
 * will be used.
 */
enum class BufferType
{
    /** Buffer contains vertex data. */
    Vertex,

    /** Buffer contains index data. */
    Index,

    /** Buffer contains constant (uniform) data. */
    Constant,

    /** Represents the number of valid entries in this enum. */
    EnumCount
};

// TODO(Marko): Add support for bit operations in enum class
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

enum class ShaderType : size_t
{
    Vertex = 0,
    Fragment,
    Geometry,
    TessellationControl,
    TessellationEvaluation,
    Compute,
    EnumCount
};

/** Controls what portion of the depth buffer is written to. */
enum class DepthMask
{
    /** Writing to the depth buffer is prohibited. */
    None = 0,

    /** Writing to the depth buffer is allowed. */
    All
};

/**
 * Controls if the primitive should be filled or just the edges should be drawn.
 */
enum class FillMode
{
    /** The primitive is filled. */
    Solid = 0,

    /** Only the edges of the primitive are drawn. */
    Wireframe
};

struct GraphicsContextDesc
{
    /**
     * Controls the debug layer of the underlying render API. Ignored in all configurations except
     * Debug.
     */
    bool enable_debug_layer = true;

    /**
     * Controls if the *HasFailed methods will report an API call fail if the warning message is
     * received from the debug layer. Ignored in all configurations except Debug.
     */
    bool should_fail_warning = true;

    /**
     * In case of workloads that last more then 2 seconds on the GPU side control if the timeout is
     * triggered or not.
     */
    bool disable_gpu_timeout = false;

    /**
     * Controls if the underlying API for creating resources is thread-safe. If this is set to false
     * the creation needs to be synchronized manually between threads. Also it is not possible to
     * create commands lists if this is set to false.
     */
    bool is_resource_creation_thread_safe = true;

    /**
     * Optional parameter representing a window handle in OS needed by some graphics API for
     * creation of the graphics context (e.g. OpenGL).
     */
    NativeWindowHandle window_handle = nullptr;

    /** OpenGL specific option. Major version of OpenGL used. */
    int gl_major_version = 4;

    /** OpenGL specific option. Minor version of OpenGL used. */
    int gl_minor_version = 6;
};

struct SwapChainDesc
{
    /** Width of the swap chain. The 0 is invalid value. */
    int32_t width = 0;

    /** Height of the swap chain. The 0 is invalid value. */
    int32_t height = 0;

    /** Pixel format of the color buffer. */
    PixelFormat color_format = PixelFormat::R8G8B8A8_UNORM;

    /** Pixel format of the depth stencil buffer. */
    PixelFormat depth_stencil_format = PixelFormat::D24_UNORM_S8_UINT;

    /** Signals if the depth stencil buffer should be used. */
    bool use_depth_stencil = false;

    /** Signals if the swap chain should be created in windowed mode. */
    bool is_windowed = true;
};

struct ShaderDesc
{
    /** Type of the shader. */
    ShaderType type;

    /** Source code of the shader. */
    String source;

    /**
     * Name of the function that represents the entry point of the shader. Can be empty for
     * OpenGL.
     */
    String entry_point;
};

struct BufferDesc
{
    /** Type of the buffer. */
    BufferType type = BufferType::Constant;

    /** Defines how the buffer should be used by the GPU. */
    Usage usage = Usage::Default;

    /** Total size of a buffer in bytes. */
    uint32_t size = 0;

    /** Size of one element, in bytes, in an array of elements. */
    uint32_t stride = 0;
};

struct SamplerDesc
{
    /** Filtering mode used when sampling the image when polygon size is smaller then the image. */
    ImageFilter min_filter = ImageFilter::Linear;

    /** Filtering mode used when sampling the image when polygon size is larger then the image. */
    ImageFilter mag_filter = ImageFilter::Linear;

    /** Filtering mode used when figuring out which mip map level to use. */
    ImageFilter mip_map_filter = ImageFilter::Linear;

    /**
     * Maximum anisotropy level to use. If larger then 1 the anisotropic filtering is active. This
     * filter can be used in combination with the min and mag filters.
     */
    uint32_t max_anisotropy = 1;

    /** Address mode used for the u image coordinate. */
    ImageAddressMode address_mode_u = ImageAddressMode::Repeat;

    /** Address mode used for the v image coordinate. */
    ImageAddressMode address_mode_v = ImageAddressMode::Repeat;

    /** Address mode used for the w image coordinate. */
    ImageAddressMode address_mode_w = ImageAddressMode::Repeat;

    /** Border color used when address mode is set to Border. */
    math::Vector4 border_color = Colors::kPink;

    /**
     * Bias to be added to the mip level before sampling. Add to shader-supplied bias, if any is
     * supplied.
     */
    float lod_bias = 0;

    /** Minimum mip level to use. */
    float base_mip_level = 0;

    /** Maximum mip level to use. */
    float max_mip_level = 1'000;

    /** Minimum LOD level to use. This value will resolve to base_mip_level value. */
    real min_lod = 0;

    /** Maximum LOD level to use. This value can't be larger then max_mip_level. */
    real max_lod = 1'000;
};

struct ImageDesc
{
    /** Width of the image in pixels. */
    int32_t width = 0;

    /** Height of the image in pixels. */
    int32_t height = 0;

    /** Number of images in the array. Ignored for ImageType::Image2D and ImageType::CubeMap. */
    int32_t array_size = 1;

    /** Type of the image. */
    ImageType type;

    /** Image pixel format. */
    PixelFormat pixel_format = PixelFormat::R8G8B8A8_UNORM_SRGB;

    /** If image should have mip maps. */
    bool use_mips = false;

    /** Sampling parameters for the image. */
    SamplerDesc sampler;
};

struct FrameBufferProperties
{
    int ColorBufferCount = 1;
    StackArray<ImageDesc, GraphicsConstants::kMaxFrameBufferColorBuffers>
        ColorBufferProperties;

    bool UseDepthStencil = false;
    ImageDesc DepthStencilBufferProperties;
};

constexpr int kAppendAlignedElement = -1;
struct InputLayoutDesc
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

struct RasterizerDesc
{
    /** Controls if the primitive should be filled or not. */
    FillMode fill_mode = FillMode::Solid;

    /** Specifies the winding order of the front face. */
    WindingOrder front_face_winding_order = WindingOrder::CCW;

    /** Specifies the face that should be culled. */
    Face cull_face = Face::Back;

    /** Multiplier of the smallest value guaranteed to produce a resolvable offset. */
    real depth_bias = 0;

    /** A multiplier of the change of depth relative to the screen area of the polygon. */
    real slope_scaled_depth_bias = 0.0;

    /** Bottom left point of a rectangle that defines the scissor test. */
    math::Point2 scissor_bottom_left;

    /**
     * Size of a rectangle that defines the scissor test. If width or height is zero the scissor
     * test is disabled.
     */
    math::Vector2 scissor_size;
};

struct DepthStencilDesc
{
    /** Helper constant for the stencil masks that allows all bits to be written or read. */
    static constexpr uint8_t k_stencil_mask_all = 0xFF;

    /** Controls if the depth test is enabled. */
    bool is_depth_enabled = false;

    /** What comparison function to use for the depth test. */
    Comparator depth_comparator = Comparator::Less;

    /** Controls if the depth buffer is writable. */
    DepthMask depth_mask = DepthMask::All;

    /** Controls if the stencil test is enabled. */
    bool is_stencil_enabled = false;

    /** Controls which bits of the stencil buffer are read. Not supported in OpenGL. */
    uint8_t stencil_read_mask = k_stencil_mask_all;

    /** Controls which bits of the stencil buffer are written. */
    uint8_t stencil_write_mask = k_stencil_mask_all;

    /** Reference value used for the stencil test. */
    int32_t stencil_ref_value = 0;

    /** Operation to perform when the stencil test fails for the front face. */
    StencilOperation stencil_front_face_fail_op = StencilOperation::Keep;

    /**
     * Operation to perform when the stencil test passes and the depth test fails for the front
     * face.
     */
    StencilOperation stencil_front_face_depth_fail_op = StencilOperation::Keep;

    /**
     * Operation to perform when the stencil test passes and the depth test passes for the front
     * face.
     */
    StencilOperation stencil_front_face_pass_op = StencilOperation::Keep;

    /** Comparison function to use for the stencil test for the front face. */
    Comparator stencil_front_face_comparator = Comparator::Never;

    /** Operation to perform when the stencil test fails for the back face. */
    StencilOperation stencil_back_face_fail_op = StencilOperation::Keep;

    /**
     * Operation to perform when the stencil test passes and the depth test fails for the back
     * face.
     */
    StencilOperation stencil_back_face_depth_fail_op = StencilOperation::Keep;

    /**
     * Operation to perform when the stencil test passes and the depth test passes for the back
     * face.
     */
    StencilOperation stencil_back_face_pass_op = StencilOperation::Keep;

    /** Comparison function to use for the stencil test for the back face. */
    Comparator stencil_back_face_comparator = Comparator::Never;
};

struct BlendDesc
{
    /** Specifies if blending is enabled. */
    bool is_enabled = true;

    /** Value used to multiply the source color. */
    BlendFactor src_color_factor = BlendFactor::SrcAlpha;

    /** Value used to multiply the destination color. */
    BlendFactor dst_color_factor = BlendFactor::InvSrcAlpha;

    /** Operation used to combine the source and destination colors to get resulting color. */
    BlendOperation color_operation = BlendOperation::Add;

    /** Value used to multiply the source alpha. */
    BlendFactor src_alpha_factor = BlendFactor::One;

    /** Value used to multiply the destination alpha. */
    BlendFactor dst_alpha_factor = BlendFactor::InvSrcAlpha;

    /** Operation used to combine the source and destination alpha to get resulting alpha. */
    BlendOperation alpha_operation = BlendOperation::Add;

    /** Const color value used in the blend operation. */
    math::Vector3 const_color;

    /** Const alpha value used in the blend operation. */
    real const_alpha = MATH_REALC(0.0);
};

struct PipelineDesc
{
    Shader* vertex_shader = nullptr;
    Shader* pixel_shader = nullptr;
    Shader* geometry_shader = nullptr;
    Shader* tesselation_control_shader = nullptr;
    Shader* tesselation_evaluation_shader = nullptr;
    Shader* compute_shader = nullptr;

    InputLayoutDesc input_layout;
    RasterizerDesc rasterizer;
    BlendDesc blend;
    DepthStencilDesc depth_stencil;
};

// Helper functions

/**
 * Get size of a pixel in bytes based on the layout.
 * @param Layout Specifies the order of channels and their size.
 * @return Returns the size of a pixel in bytes.
 */
int GetPixelSize(PixelFormat Format);

}  // namespace Rndr
