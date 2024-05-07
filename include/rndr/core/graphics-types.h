#pragma once

#include "rndr/core/colors.h"
#include "rndr/core/types.h"
#include "opal/container/array.h"
#include "rndr/core/containers/ref.h"
#include "rndr/core/containers/span.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/math.h"
#include "rndr/core/platform/windows-forward-def.h"

namespace Rndr
{

class Shader;

struct GraphicsConstants
{
    static constexpr i32 k_max_frame_buffer_color_buffers = 4;
    static constexpr i32 k_max_input_layout_entries = 32;
    static constexpr i32 k_max_shader_resource_bind_slots = 128;
    static constexpr i32 k_max_constant_buffers = 16;
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
 * UNORM - Interpreted by a shader as f32ing-poi32 value in the range [0, 1].
 * SNORM - Interpreted by a shader as f32ing-poi32 value in the range [-1, 1].
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

    R8G8B8_UNORM,
    R8G8B8_UNORM_SRGB,
    R8G8B8_UINT,
    R8G8B8_SNORM,
    R8G8B8_SINT,

    R8G8_UNORM,
    R8G8_UNORM_SRGB,  // TODO: This format can probably be removed
    R8G8_UINT,
    R8G8_SNORM,
    R8G8_SINT,

    R8_UNORM,
    R8_UNORM_SRGB,  // TODO: This format can probably be removed
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

    R16G16_FLOAT,

    /** Represents number of elements in the enum. */
    EnumCount
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
    /**  Use this when you don't need to update the resource after creation. Does not allow reading. */
    Default = 0,

    /** Used when we need to update resource frequently, such as constant buffers that store a view matrix. Does not allow reading. */
    Dynamic,

    /** Used when we need to move data from the GPU to the CPU side. These buffers can only be written to on creation. */
    ReadBack,

    /** Represents number of elements in the enum. */
    EnumCount
};

/**
 * Represents the winding order of a triangle. This is used to determine the front face of a
 * triangle.
 */
enum class WindingOrder : u32
{
    /** Clockwise winding order. */
    CW = 0,

    /** Counter-clockwise winding order. */
    CCW
};

/**
 * Represents the faces of a triangle that are visible to the camera.
 */
enum class Face : u32
{
    /** Both front and back faces are visible. */
    None = 0,

    /** Only front faces are visible. */
    Front,

    /** Only back faces are visible. */
    Back
};

/**
 * Represents the type of a primitive topology. This is used to determine how the vertices are
 * interpreted.
 */
enum class PrimitiveTopology
{
    /** Each vertex represents one point. */
    Point = 0,

    /** Use two vertices two form a line. */
    Line,

    /**
     * Use two vertices to form a line, where the second vertex is the first vertex of the next
     * line.
     */
    LineStrip,

    /** Use three vertices to form a triangle. */
    Triangle,

    /**
     * Use three vertices to form a triangle, where the second and third vertices are the first
     * two vertices of the next triangle.
     */
    TriangleStrip,

    /** Represents number of elements in the enum. */
    EnumCount
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

    /** Buffer contains shader storage data. */
    ShaderStorage,

    /** Represents the number of valid entries in this enum. */
    EnumCount
};

// TODO(Marko): Add support for bit operations in enum class
namespace ImageBindFlags
{
enum : u32
{
    RenderTarget = 1 << 0,
    DepthStencil = 1 << 1,
    ShaderResource = 1 << 2,
    UnorderedAccess = 1 << 3,
};
}

/**
 * Represents how image can be accessed from the shader.
 */
enum class ImageAccess
{
    Read,
    Write,
    ReadWrite
};

enum class DataRepetition
{
    PerVertex,
    PerInstance
};

enum class ShaderType : u8
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
     * Optional parameter representing a window handle in OS needed by some graphics API for
     * creation of the graphics context (e.g. OpenGL).
     */
    NativeWindowHandle window_handle = nullptr;
};

struct SwapChainDesc
{
    /** Width of the swap chain. The 0 is invalid value. */
    i32 width = 0;

    /** Height of the swap chain. The 0 is invalid value. */
    i32 height = 0;

    /** Pixel format of the color buffer. */
    PixelFormat color_format = PixelFormat::R8G8B8A8_UNORM;

    /** Pixel format of the depth stencil buffer. */
    PixelFormat depth_stencil_format = PixelFormat::D24_UNORM_S8_UINT;

    /** Signals if the depth stencil buffer should be used. */
    bool use_depth_stencil = false;

    /** Signals if the swap chain should be created in windowed mode. */
    bool is_windowed = true;

    /** Signals if the swap chain should be created with vsync enabled. */
    bool enable_vsync = true;
};

struct ShaderDesc
{
    /** Type of the shader. */
    ShaderType type;

    /** Source code of the shader. */
    String source;

    /**
     * Name of the function that represents the entry poi32 of the shader. Can be empty for
     * OpenGL.
     */
    String entry_point;

    /** List of defines that should be added to the shader in a form of DEFINE_NAME VALUE or just DEFINE_NAME. */
    Opal::Array<String> defines;
};

struct BufferDesc
{
    /** Type of the buffer. */
    BufferType type = BufferType::Constant;

    /** Defines how the buffer should be used by the GPU. */
    Usage usage = Usage::Default;

    /** Total size of a buffer in bytes. */
    st size = 0;

    /** Size of one element, in bytes, in an array of elements. */
    i64 stride = 0;

    /** Offset, in bytes, from the beginning of the buffer to the first element to use. */
    i64 offset = 0;
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
    f32 max_anisotropy = 1.0f;

    /** Address mode used for the u image coordinate. */
    ImageAddressMode address_mode_u = ImageAddressMode::Repeat;

    /** Address mode used for the v image coordinate. */
    ImageAddressMode address_mode_v = ImageAddressMode::Repeat;

    /** Address mode used for the w image coordinate. */
    ImageAddressMode address_mode_w = ImageAddressMode::Repeat;

    /** Border color used when address mode is set to Border. */
    Vector4f border_color = Colors::k_pink;

    /**
     * Bias to be added to the mip level before sampling. Add to shader-supplied bias, if any is
     * supplied.
     */
    f32 lod_bias = 0;

    /** Minimum mip level to use. */
    i32 base_mip_level = 0;

    /**
     * Maximum mip level to use. If use_mips in ImageDesc is set to true and this value is 0 it will be overridden by number of mip map
     * levels - 1.
     */
    i32 max_mip_level = 0;

    /** Minimum LOD level to use. This value will resolve to base_mip_level value. */
    f32 min_lod = 0.0f;

    /** Maximum LOD level to use. This value can't be larger then max_mip_level. */
    f32 max_lod = 0.0f;
};

struct ImageDesc
{
    /** Width of the image in pixels. */
    i32 width = 0;

    /** Height of the image in pixels. */
    i32 height = 0;

    /** Number of images in the array. Ignored for ImageType::Image2D and ImageType::CubeMap. */
    i32 array_size = 1;

    /** Type of the image. */
    ImageType type = ImageType::Image2D;

    /** Image pixel format. */
    PixelFormat pixel_format = PixelFormat::R8G8B8A8_UNORM_SRGB;

    /** If image should have mip maps. */
    bool use_mips = false;

    /** If image should be made bindless. */
    bool is_bindless = false;

    /** Sampling parameters for the image. */
    SamplerDesc sampler;
};

struct FrameBufferProperties
{
    i32 color_buffer_count = 1;
    StackArray<ImageDesc, GraphicsConstants::k_max_frame_buffer_color_buffers> color_buffer_properties;

    bool use_depth_stencil = false;
    ImageDesc depth_stencil_buffer_properties;
};

/**
 * Describes a single element in the input layout. Input layout is a description of the vertex
 * and instance buffer data. It tells the graphics API how to interpret the data in the buffers.
 *
 * For example, vertex data consists of position, normal and texture coordinates. Each of these is
 * represented by one element in the input layout.
 */
struct InputLayoutElement
{
    /** Data format of the element. */
    PixelFormat format = PixelFormat::R32G32B32_FLOAT;

    /**
     * Offset of the element in bytes from the start of the element group. You can use
     * k_append_element to set offset where the previous element ends.
     */
    i32 offset_in_vertex = 0;

    /** Slot at which the containing vertex buffer is bound. */
    i32 binding_index = 0;

    /** Is this element present for each vertex or each instance. */
    DataRepetition repetition = DataRepetition::PerVertex;

    /** In case data is repeated on instance level this allows us to skip some instances. */
    i32 instance_step_rate = 0;
};

/**
 * Describes the input layout of a pipeline. Input layout is a description of the vertex
 * and instance buffer data. It tells the graphics API how to interpret the data in the buffers.
 * It contains data description for all buffers used by the pipeline.
 */
class Buffer;
struct InputLayoutDesc
{
    /** Index buffer used by the pipeline. If it is set to null you have to use DrawVertices. */
    Ref<const Buffer> index_buffer;

    /** List of vertex buffers used by the pipeline. */
    Opal::Array<Ref<const Buffer>> vertex_buffers;

    /** List of binding slots to which the corresponding vertex buffer is bound. */
    Opal::Array<i32> vertex_buffer_binding_slots;

    /** List of input layout elements for the data in the vertex buffers. */
    Opal::Array<InputLayoutElement> elements;
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
    f32 depth_bias = 0;

    /** A multiplier of the change of depth relative to the screen area of the polygon. */
    f32 slope_scaled_depth_bias = 0.0;

    /** Bottom left poi32 of a rectangle that defines the scissor test. */
    Point2f scissor_bottom_left = Point2f(0.0f, 0.0f);

    /**
     * Size of a rectangle that defines the scissor test. If width or height is zero the scissor
     * test is disabled.
     */
    Vector2f scissor_size = Vector2f(0.0f, 0.0f);
};

struct DepthStencilDesc
{
    /** Helper constant for the stencil masks that allows all bits to be written or read. */
    static constexpr u8 k_stencil_mask_all = 0xFF;

    /** Controls if the depth test is enabled. */
    bool is_depth_enabled = false;

    /** What comparison function to use for the depth test. */
    Comparator depth_comparator = Comparator::Less;

    /** Controls if the depth buffer is writable. */
    DepthMask depth_mask = DepthMask::All;

    /** Controls if the stencil test is enabled. */
    bool is_stencil_enabled = false;

    /** Controls which bits of the stencil buffer are read. Not supported in OpenGL. */
    u8 stencil_read_mask = k_stencil_mask_all;

    /** Controls which bits of the stencil buffer are written. */
    u8 stencil_write_mask = k_stencil_mask_all;

    /** Reference value used for the stencil test. */
    i32 stencil_ref_value = 0;

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
    Vector3f const_color = Vector3f(0.0f, 0.0f, 0.0f);

    /** Const alpha value used in the blend operation. */
    f32 const_alpha = 0.0f;
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

struct FrameBufferDesc
{
    Opal::Array<ImageDesc> color_attachments;
    ImageDesc depth_stencil_attachment;
    bool use_depth_stencil = false;
};

/**
 * Helper struct to be used for creating indirect draw calls when there is no indices.
 */
struct DrawVerticesData
{
    /** Number of vertices to draw. */
    u32 vertex_count;
    /** Number of instances to draw. */
    u32 instance_count;
    /** Offset of the first vertex to draw in vertices relative to the vertex buffer. */
    u32 first_vertex;
    /** Offset of the first instance to draw in instances relative to the instance buffer. */
    u32 base_instance;
};

/**
 * Helper struct to be used for creating indirect draw calls when indices are used.
 */
struct DrawIndicesData
{
    /** Number of indices to draw. */
    u32 index_count;
    /** Number of instances to draw. */
    u32 instance_count;
    /** Offset of the first index to draw in indices relative to the index buffer. */
    u32 first_index;
    /** Offset of the first vertex to draw in vertices relative to the vertex buffer. */
    u32 base_vertex;
    /** Offset of the first instance to draw in instances relative to the instance buffer. */
    u32 base_instance;
};

// Helper functions

/**
 * Returns the size of a pixel in a given pixel format in bytes.
 * @param format Pixel format.
 * @return Size of a pixel in bytes.
 */
i32 FromPixelFormatToPixelSize(PixelFormat format);

/**
 * Returns the number of components in a given pixel format.
 * @param format Pixel format.
 * @return Number of components in a given pixel format.
 */
i32 FromPixelFormatToComponentCount(PixelFormat format);

/**
 * Check if for given pixel format components are stored in one byte.
 * @param format Pixel format.
 * @return True if for given pixel format components are stored in one byte.
 */
bool IsComponentLowPrecision(PixelFormat format);

/**
 * Check if for given pixel format components are stored in 4 bytes.
 * @param format Pixel format.
 * @return True if for given pixel format components are stored in 4 bytes.
 */
bool IsComponentHighPrecision(PixelFormat format);

/**
 * Returns the name of the enum field in BufferType enum as a string.
 * @param type Buffer type.
 * @return Name of the enum field in BufferType enum as a string or empty string if the value is not supported.
 */
Rndr::String FromBufferTypeToString(BufferType type);

}  // namespace Rndr
