#pragma once

#include <cstring>

#include "opal/container/string.h"
#include "opal/enum-flags.h"

#include "rndr/pixel-format.hpp"
#include "rndr/types.hpp"

/**
 * The vocabulary of Forge: the enums that describe what a resource is and how it is used.
 *
 * These are Vulkan concepts - image layouts, pipeline stages, usage masks - rather than concepts every
 * graphics API has, which is why they live here and not in rndr/graphics-types.hpp. Types that name
 * something an OpenGL renderer would recognize too, such as PixelFormat, ShaderTypeBits and IndexSize,
 * stay shared.
 *
 * The values of the flag enums mirror the Vulkan values they map to, so translating them is a cast. The
 * plain enums are translated by a switch in the source file that needs them, so a value that has no Vulkan
 * counterpart cannot be cast into one by accident.
 */

namespace Rndr::Forge
{

/** How a buffer is allowed to be used. Mirrors VkBufferUsageFlagBits. */
enum class BufferUsageBits : u32
{
    None = 0,
    /** Source of a transfer command. */
    TransferSource = 0x00000001,
    /** Destination of a transfer command. */
    TransferDestination = 0x00000002,
    /** Bound as a uniform buffer, read-only and small. */
    ConstantBuffer = 0x00000010,
    /** Bound as a storage buffer, read-write and large. */
    StorageBuffer = 0x00000020,
    /** Bound with CmdBindIndexBuffer. */
    IndexBuffer = 0x00000040,
    /** Bound with CmdBindVertexBuffer. */
    VertexBuffer = 0x00000080,
    /** Holds the arguments of an indirect draw or dispatch. */
    IndirectBuffer = 0x00000100
};
OPAL_ENUM_CLASS_FLAGS(BufferUsageBits);

/**
 * How the host intends to touch the memory of a buffer. This picks the memory type the allocation lands in.
 * The two host-visible values differ in speed rather than in what is allowed - reading write-combined memory
 * works and crawls - while None asks for memory the host cannot reach at all.
 */
enum class HostAccess : u8
{
    /** Written from start to end and never read. Write-combined memory is fine, which is usually fastest. */
    SequentialWrite,
    /** Read back, or written out of order. Cached memory, which is what a host read needs. */
    Random,
    /**
     * Not touched by the host at all, so the allocation is free to land in memory the host cannot map.
     * Buffer::Update and ::Read throw on such a buffer; fill and read it with UploadToBuffer and
     * ReadBackBuffer from rndr/forge/transfer.hpp instead.
     */
    None
};

/** How a texture is allowed to be used. Mirrors VkImageUsageFlagBits. */
enum class TextureUsageBits : u32
{
    None = 0,
    /** Source of a transfer command, which includes reading it back and generating its mips. */
    TransferSource = 0x00000001,
    /** Destination of a transfer command, which is how a texture is uploaded. */
    TransferDestination = 0x00000002,
    /** Sampled in a shader. */
    Sampled = 0x00000004,
    /** Read or written in a shader without a sampler. */
    Storage = 0x00000008,
    /** Rendered into as a color attachment. */
    ColorAttachment = 0x00000010,
    /** Rendered into as a depth or stencil attachment. */
    DepthStencilAttachment = 0x00000020,
    /** Backed lazily, for an attachment that never leaves tile memory. */
    TransientAttachment = 0x00000040,
    /** Read in a fragment shader at the fragment being written, within the same render pass. */
    InputAttachment = 0x00000080
};
OPAL_ENUM_CLASS_FLAGS(TextureUsageBits);

/** Number of dimensions the image data has. */
enum class TextureDimension : u8
{
    Texture1D,
    Texture2D,
    Texture3D
};

/** How a view interprets the image it was created from. */
enum class TextureViewType : u8
{
    Texture1D,
    Texture2D,
    Texture3D,
    Cube,
    Texture1DArray,
    Texture2DArray,
    CubeArray
};

/**
 * The scalar a specialization constant holds. Mirrors what SPIR-V allows one to be, except that a constant
 * narrower than 32 bits is reported as its 32 bit counterpart so a caller can write a plain integer for it;
 * SpecializationConstantInfo::byte_size keeps the width Vulkan is owed.
 */
enum class SpecializationType : u8
{
    Bool,
    Int32,
    UInt32,
    Float32,
    Int64,
    UInt64,
    Float64
};

/**
 * One specialization constant value. Every kind of them fits in eight bytes, so this is a tag and a bit
 * pattern rather than a variant - the raw bytes are what VkSpecializationInfo wants either way.
 *
 * The constructors are implicit so a call site reads as {"MAX_LIGHTS", 8} without naming a type.
 */
struct SpecializationValue
{
    SpecializationType type = SpecializationType::Int32;
    /** The bit pattern, low-order word first, which is how SPIR-V stores it. */
    u64 bits = 0;

    SpecializationValue() = default;
    SpecializationValue(bool value) : type(SpecializationType::Bool), bits(value ? 1u : 0u) {}
    SpecializationValue(i32 value) : type(SpecializationType::Int32) { Store(&value, sizeof(value)); }
    SpecializationValue(u32 value) : type(SpecializationType::UInt32) { Store(&value, sizeof(value)); }
    SpecializationValue(f32 value) : type(SpecializationType::Float32) { Store(&value, sizeof(value)); }
    SpecializationValue(i64 value) : type(SpecializationType::Int64) { Store(&value, sizeof(value)); }
    SpecializationValue(u64 value) : type(SpecializationType::UInt64) { Store(&value, sizeof(value)); }
    SpecializationValue(f64 value) : type(SpecializationType::Float64) { Store(&value, sizeof(value)); }

    /**
     * A pointer would otherwise reach the bool constructor, so {"MAX_LIGHTS", "8"} would compile and mean
     * true. Deleted rather than left to convert, since nothing downstream can tell that apart from a bool
     * the caller meant.
     */
    template <typename T>
    SpecializationValue(T*) = delete;

private:
    /** Copies the bit pattern in without reinterpreting it, which a cast through a wider type would. */
    void Store(const void* source, u64 size)
    {
        bits = 0;
        memcpy(&bits, source, size);
    }
};

/** A constant a shader declares, named, and the value to build a pipeline with. */
struct SpecializationConstant
{
    Opal::StringUtf8 name;
    SpecializationValue value;
};

/**
 * Which colour channels a pipeline is allowed to write. Mirrors VkColorComponentFlagBits, so a mask is a
 * cast. Masking a channel off leaves whatever the attachment already held in it, which is not the same as
 * writing zero - a pass that only wants alpha leaves the colour of the pass before it alone.
 */
enum class ColorWriteMaskBits : u8
{
    None = 0,
    Red = 0x1,
    Green = 0x2,
    Blue = 0x4,
    Alpha = 0x8,
    /** Every channel, which is what a pipeline that says nothing about this gets. */
    All = Red | Green | Blue | Alpha
};
OPAL_ENUM_CLASS_FLAGS(ColorWriteMaskBits);

/**
 * Which faces a stencil command applies to. Mirrors VkStencilFaceFlagBits.
 *
 * Not Rndr::Face, which is a cull mode: there None means "cull nothing" and there is no way to name both
 * faces at once, which is the case a stencil command wants most.
 */
enum class StencilFaceBits : u8
{
    Front = 0x1,
    Back = 0x2,
    FrontAndBack = Front | Back
};
OPAL_ENUM_CLASS_FLAGS(StencilFaceBits);

/**
 * State a pipeline leaves to the command buffer instead of baking in, so one pipeline serves draws that
 * differ only in these. Viewport and scissor are always dynamic - every pipeline Forge builds declares them
 * and CmdSetViewport and CmdSetScissor are the only way to give them a value - so they are not listed here.
 *
 * A state named here has to be set with the matching Cmd* call before a draw that uses it. One set here and
 * never set on the command buffer is undefined, which is what makes this a mask rather than a default.
 */
enum class DynamicStateBits : u8
{
    None = 0,
    /** RasterizerDesc::depth_bias_* are ignored and CmdSetDepthBias supplies them. */
    DepthBias = 0x1,
    /** The stencil comparison value, which CmdSetStencilReference supplies. */
    StencilReference = 0x2,
    /** Line width, which CmdSetLineWidth supplies. Above one needs DeviceFeatures::wide_lines. */
    LineWidth = 0x4
};
OPAL_ENUM_CLASS_FLAGS(DynamicStateBits);

enum class SampleCount : u8
{
    Count1,
    Count2,
    Count4,
    Count8,
    Count16,
    Count32,
    Count64
};

/** How the presentation engine picks which image to display and whether it waits for the vertical blank. */
enum class PresentMode : u8
{
    /** Present immediately, without waiting for the vertical blank. Tears, lowest latency. */
    Immediate,
    /** Wait for the vertical blank, replacing anything already queued. Does not tear, no frame rate cap. */
    Mailbox,
    /** Wait for the vertical blank, queueing every image. Does not tear, always supported. */
    Fifo,
    /** Like Fifo, but presents immediately when the queue ran dry and the vertical blank has passed. */
    FifoRelaxed
};

/** How the presentation engine interprets the values written into a swap chain image. */
enum class ColorSpace : u8
{
    /** sRGB primaries with the sRGB transfer function. What a non-HDR display expects. */
    SrgbNonlinear,
    /** sRGB primaries with a linear transfer function, extended past [0, 1]. */
    ExtendedSrgbLinear,
    /** Rec. 2020 primaries with the ST.2084 (PQ) transfer function. HDR10. */
    Hdr10St2084,
    /** Display-P3 primaries with the sRGB transfer function. */
    DisplayP3Nonlinear
};

/** What an image is being used for right now. Mirrors VkImageLayout. */
enum class ImageLayout
{
    // Usually used for initial layout of an image or when we don't care about layout.
    Undefined = 0,
    // When image is used for multiple incompatible operations, such as reads and writes
    // from different pipeline stages. Slowest type of layout.
    General = 1,
    // When image is used as a render target.
    ColorAttachment = 2,
    // When image is used as a depth/stencil attachment in the frame buffer.
    DepthStencilAttachment = 3,
    // When image is used for reading depth/stencil values in shaders while its also bound
    // for depth testing.
    DepthStencilReadOnly = 4,
    // When image is sampled in any shader stage.
    ShaderReadOnly = 5,
    // When image is used as a source for image transfer using DMA engine.
    TransferSource = 6,
    // When image is used as a destination for image transfer using DMA engine.
    TransferDestination = 7,
    // When image is a swap chain image that is to be presented to the screen.
    Present = 1000001002,
};

/** Which part of the data of an image a command refers to. Mirrors VkImageAspectFlagBits. */
enum class ImageAspectBits : u8
{
    /** Left for the aspect to be derived from the format of the texture. */
    None = 0,
    Color = 1,
    Depth = 2,
    Stencil = 4
};
OPAL_ENUM_CLASS_FLAGS(ImageAspectBits);

/** Everything from the offset to the end of the buffer. Mirrors VK_WHOLE_SIZE. */
static constexpr u64 k_whole_buffer = 0xFFFFFFFFFFFFFFFF;

/** Every mip level from first_mip_level on. Mirrors VK_REMAINING_MIP_LEVELS. */
static constexpr u32 k_all_mip_levels = 0xFFFFFFFF;

/** Every array layer from first_array_layer on. Mirrors VK_REMAINING_ARRAY_LAYERS. */
static constexpr u32 k_all_array_layers = 0xFFFFFFFF;

/** Every query of a pool from first_query on. Vulkan has no counterpart; the count is resolved here. */
static constexpr u32 k_all_queries = 0xFFFFFFFF;

/**
 * The aspect an explicit mask names, or the one the format implies when the mask is empty: depth, stencil,
 * both, or color. Shared by everything that names part of an image - ranges, copy regions, blit regions.
 */
[[nodiscard]] inline ImageAspectBits ResolveAspectMask(ImageAspectBits aspect_mask, PixelFormat format)
{
    if (aspect_mask != ImageAspectBits::None)
    {
        return aspect_mask;
    }
    ImageAspectBits resolved = ImageAspectBits::None;
    if (IsDepthFormat(format))
    {
        resolved |= ImageAspectBits::Depth;
    }
    if (IsStencilFormat(format))
    {
        resolved |= ImageAspectBits::Stencil;
    }
    return resolved == ImageAspectBits::None ? ImageAspectBits::Color : resolved;
}

/**
 * Which part of a texture a command refers to. The whole texture by default: every mip level, every array
 * layer, and whichever aspect its format has.
 */
struct ImageSubresourceRange
{
    /** Which aspect of the image we care about. Empty derives it from the format. */
    ImageAspectBits aspect_mask = ImageAspectBits::None;
    u32 first_mip_level = 0;
    u32 mip_level_count = k_all_mip_levels;
    u32 first_array_layer = 0;
    u32 array_layer_count = k_all_array_layers;

    /**
     * The aspect this range names for a texture of the given format. Returns aspect_mask when it was set,
     * and otherwise the aspect the format implies: depth, stencil, both, or color.
     */
    [[nodiscard]] ImageAspectBits ResolveAspectMask(PixelFormat format) const
    {
        return Forge::ResolveAspectMask(aspect_mask, format);
    }
};

/** Stages of the pipeline a barrier can wait on or block. Mirrors VkPipelineStageFlagBits2. */
enum class PipelineStageBits : u64
{
    None = 0ull,
    /** Nothing has run yet. Where a barrier that waits for nothing puts its source. */
    PipelineStart = 0x1ull,
    IndirectDraw = 0x2ull,
    /** Reading indices and vertex attributes both. IndexInput and VertexAttributeInput are its halves. */
    VertexInput = 0x4ull,
    VertexShader = 0x8ull,
    TessellationControlShader = 0x10ull,
    TessellationEvaluationShader = 0x20ull,
    GeometryShader = 0x40ull,
    FragmentShader = 0x80ull,
    EarlyFragmentTests = 0x100ull,
    LateFragmentTests = 0x200ull,
    ColorAttachmentOutput = 0x400ull,
    ComputeShader = 0x800ull,
    /** Every kind of transfer. Copy, Blit, Resolve and Clear are its parts, for a barrier that knows which. */
    Transfer = 0x1000ull,
    /** Nothing is left to run. Where work waits for a barrier that only has to happen before the end. */
    PipelineEnd = 0x2000ull,
    /** The host reading or writing mapped memory. */
    Host = 0x4000ull,
    /** Every stage a graphics pipeline has. */
    AllGraphics = 0x8000ull,
    /** Every stage. The sledgehammer, correct everywhere and never the fastest. */
    AllCommands = 0x10000ull,
    /** Needs VK_EXT_mesh_shader, which DeviceFeatures::task_shader enables. */
    TaskShader = 0x80000ull,
    /** Needs VK_EXT_mesh_shader, which DeviceFeatures::mesh_shader enables. */
    MeshShader = 0x100000ull,
    Copy = 0x100000000ull,
    Resolve = 0x200000000ull,
    Blit = 0x400000000ull,
    Clear = 0x800000000ull,
    IndexInput = 0x1000000000ull,
    VertexAttributeInput = 0x2000000000ull,
    /** Every stage before rasterization: vertex, tessellation, geometry, and task and mesh where they run. */
    PreRasterizationShaders = 0x4000000000ull
};
OPAL_ENUM_CLASS_FLAGS(PipelineStageBits);

/**
 * How a stage touches the memory a barrier covers. Mirrors VkAccessFlagBits2.
 *
 * Read and Write are "any read" and "any write". They are correct beside any stage and are what a barrier
 * written by hand should reach for first, since an access that does not match its stage is invalid and the
 * pair can never be wrong. The named ones below say which read or which write, which is the narrowing
 * synchronization2 exists for: a driver told "color attachment write" can leave everything else alone.
 * Forge's own barrier presets use the named ones, since a preset knows exactly what the texture is for.
 */
enum class PipelineStageAccessBits : u64
{
    None = 0ull,
    IndirectCommandRead = 0x1ull,
    IndexRead = 0x2ull,
    VertexAttributeRead = 0x4ull,
    ConstantBufferRead = 0x8ull,
    InputAttachmentRead = 0x10ull,
    ShaderRead = 0x20ull,
    ShaderWrite = 0x40ull,
    ColorAttachmentRead = 0x80ull,
    ColorAttachmentWrite = 0x100ull,
    DepthStencilAttachmentRead = 0x200ull,
    DepthStencilAttachmentWrite = 0x400ull,
    TransferRead = 0x800ull,
    TransferWrite = 0x1000ull,
    HostRead = 0x2000ull,
    HostWrite = 0x4000ull,
    /** Any read at all. Correct beside every stage. */
    Read = 0x8000ull,
    /** Any write at all. Correct beside every stage. */
    Write = 0x10000ull,
    ShaderSampledRead = 0x100000000ull,
    ShaderStorageRead = 0x200000000ull,
    ShaderStorageWrite = 0x400000000ull
};
OPAL_ENUM_CLASS_FLAGS(PipelineStageAccessBits)

/** Everything a queue family index can be when a barrier is not transferring ownership. Mirrors VK_QUEUE_FAMILY_IGNORED. */
static constexpr u32 k_ignored_queue_family = 0xFFFFFFFF;

/** What a dependency covers beyond the resources it names. Mirrors VkDependencyFlagBits. */
enum class DependencyFlagBits : u8
{
    None = 0,
    /**
     * Each region of the attachment depends only on the same region of what came before, which lets a tiled
     * device keep the work in tile memory. Only valid inside a render pass.
     */
    ByRegion = 1
};
OPAL_ENUM_CLASS_FLAGS(DependencyFlagBits)

}  // namespace Rndr::Forge
