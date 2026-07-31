#pragma once

#include "opal/enum-flags.h"

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

/** Samples per pixel of a multisampled texture. */
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
    Color = 1,
    Depth = 2,
    Stencil = 4
};
OPAL_ENUM_CLASS_FLAGS(ImageAspectBits);

struct ImageSubresourceRange
{
    // Which aspect of the image we care about.
    ImageAspectBits aspect_mask = ImageAspectBits::Color;
    u32 first_mip_level = 0;
    u32 mip_level_count = 1;
    u32 first_array_layer = 0;
    u32 array_layer_count = 1;
};

/** Stages of the pipeline a barrier can wait on or block. Mirrors VkPipelineStageFlagBits2. */
enum class PipelineStageBits : u64
{
    None = 0ull,
    PipelineStart = 0x1ull,
    IndirectDraw = 0x2ull,
    VertexInput = 0x4ull,
    VertexShader = 0x8ull,
    FragmentShader = 0x80ull,
    EarlyFragmentTests = 0x100ull,
    LateFragmentTests = 0x200ull,
    ColorAttachmentOutput = 0x400ull,
    ComputeShader = 0x800ull,
    Transfer = 0x1000ull
};
OPAL_ENUM_CLASS_FLAGS(PipelineStageBits);

/** How a stage touches the memory a barrier covers. Mirrors VkAccessFlagBits2. */
enum class PipelineStageAccessBits : u64
{
    None = 0ull,
    Read = 0x8000ull,
    Write = 0x10000ull
};
OPAL_ENUM_CLASS_FLAGS(PipelineStageAccessBits)

}  // namespace Rndr::Forge
