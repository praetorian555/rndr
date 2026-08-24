#pragma once

#include "volk/volk.h"

#include "opal/clonable-base.h"
#include "opal/container/expected.h"
#include "opal/container/optional.h"
#include "opal/container/ref.h"
#include "opal/container/string.h"
#include "opal/variant.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/types.hpp"

namespace Rndr::Forge
{

enum class AttachmentLoadOperation : u8
{
    Load,
    Clear,
    DontCare
};

enum class AttachmentStoreOperation : u8
{
    Store,
    DontCare
};

/** What a Clear load operation writes into a depth or a stencil attachment. */
struct DepthStencilClearValue
{
    f32 depth = 0.0f;
    u32 stencil = 0;
};

struct RenderingAttachmentDesc : Opal::ClonableBase<RenderingAttachmentDesc>
{
    /**
     * Texture to render into. Its image view is what the attachment names, and the layout it is rendered in
     * is the one the texture tracks - so a pass says which texture it draws into and nothing else, and a
     * layout that disagrees with the barriers that led up to it cannot be written here at all.
     *
     * Every level and layer the view covers has to be in one layout, and that layout has to be one the role
     * allows: ColorAttachment or General for a colour attachment, DepthStencilAttachment,
     * DepthStencilReadOnly or General for the other two. Anything else is refused, Undefined included - which is
     * a texture no barrier has moved into place yet.
     *
     * Empty for an attachment nobody filled in, which is refused rather than rendering into nothing.
     */
    Opal::Ref<const Texture> texture;
    AttachmentLoadOperation load_operation = AttachmentLoadOperation::Clear;
    AttachmentStoreOperation store_operation = AttachmentStoreOperation::Store;
    /**
     * What a Clear load operation writes into the attachment: a Vector4f for a colour attachment, a
     * DepthStencilClearValue for a depth or a stencil one. Read only when `load_operation` is Clear, so an
     * attachment that loads or discards leaves the default alone whichever role it plays.
     *
     * A variant rather than a union because a union cannot say which member was written - which is the one
     * misuse the validation layer cannot catch either, `VkClearValue` being the same union. Writing the kind
     * the role does not use is refused at CmdBeginRendering instead of clearing to whatever the other member's
     * bytes happen to mean.
     */
    Opal::Variant<Vector4f, DepthStencilClearValue> clear_value = Vector4f{0.0f, 0.0f, 0.0f, 1.0f};

    OPAL_CLONE_FIELDS(texture, load_operation, store_operation, clear_value);
};

struct RenderingDesc
{
    Vector2i render_area_extent = {0, 0};
    Opal::DynamicArray<RenderingAttachmentDesc> color_attachments;
    /**
     * Depth attachment, absent for a pass that renders without one. Absent is the default, so a pass that
     * needs no depth says nothing rather than filling in a desc whose empty texture means "ignore this".
     * Present with no texture is a mistake and is refused.
     */
    Opal::Optional<RenderingAttachmentDesc> depth_attachment;
    /**
     * Stencil attachment, absent for a pass that does no stencil work. Vulkan takes the two sides separately
     * even when one texture carries both, so a combined format such as D24_UNORM_S8_UINT names the *same
     * texture* here as the depth attachment does - and takes its own load and store operations, since
     * clearing the depth and keeping the stencil is a thing a pass may want. A separate stencil texture names
     * its own.
     *
     * Present with no texture is a mistake and is refused, the way the depth attachment does.
     */
    Opal::Optional<RenderingAttachmentDesc> stencil_attachment;
};

/**
 * The three group counts of one indirect dispatch, laid out the way CmdDispatchIndirect reads them out of a
 * buffer. Write this into the buffer rather than three loose integers, so that the layout stays in one place.
 */
struct DispatchIndirectCommand
{
    u32 group_count_x = 0;
    u32 group_count_y = 0;
    u32 group_count_z = 0;
};

/** The arguments of one indirect draw, laid out the way CmdDrawIndirect reads them out of a buffer. */
struct DrawIndirectCommand
{
    u32 vertex_count = 0;
    u32 instance_count = 0;
    u32 first_vertex = 0;
    u32 first_instance = 0;
};

/** The arguments of one indirect indexed draw, laid out the way CmdDrawIndexedIndirect reads them. */
struct DrawIndexedIndirectCommand
{
    u32 index_count = 0;
    u32 instance_count = 0;
    u32 first_index = 0;
    /** Added to every index before it reaches the vertex buffer. Signed, unlike the rest. */
    i32 vertex_offset = 0;
    u32 first_instance = 0;
};

/** Barriers of every kind that belong to one dependency, for CommandBuffer::CmdBarriers. */
struct Barriers
{
    Opal::ArrayView<const MemoryBarrier> memory;
    Opal::ArrayView<const BufferBarrier> buffer;
    Opal::ArrayView<const TextureBarrier> texture;
    /** What the dependency covers beyond the resources above. ByRegion is only valid inside a render pass. */
    DependencyFlagBits flags = DependencyFlagBits::None;
};

/** One range of one buffer copied into one range of another. */
struct BufferCopyRegion
{
    u64 source_offset = 0;
    u64 destination_offset = 0;
    /** As much as both buffers have left past their offsets, by default. */
    u64 size = k_whole_buffer;
};

/**
 * The part of a texture one copy or blit region touches. Unlike ImageSubresourceRange this names a single
 * mip level, because a copy reads or writes one level at a time.
 */
struct ImageSubresourceLayers
{
    /** Which aspect of the image we care about. Empty derives it from the format. */
    ImageAspectBits aspect_mask = ImageAspectBits::None;
    u32 mip_level = 0;
    u32 first_array_layer = 0;
    u32 array_layer_count = 1;
};

/**
 * One region copied between a buffer and a texture, in either direction. The buffer side is a linear run of
 * pixels and the texture side is a box inside one mip level.
 */
struct BufferTextureCopyRegion
{
    u64 buffer_offset = 0;
    /** Pixels per row in the buffer. Zero means the rows are packed to the width of texture_extent. */
    u32 buffer_row_length = 0;
    /** Rows per array layer in the buffer. Zero means they are packed to the height of texture_extent. */
    u32 buffer_layer_height = 0;
    ImageSubresourceLayers texture_subresource;
    Vector3i texture_offset = {0, 0, 0};
    /** Zero on an axis means the rest of the mip level past texture_offset on that axis. */
    Vector3i texture_extent = {0, 0, 0};
};

/**
 * One box of one mip level of a texture stretched into a box of another. Unlike a copy, the two boxes need
 * not be the same size or the same format - a blit resamples and converts.
 *
 * A negative extent runs the box backwards from its offset, which is how an axis is mirrored.
 */
struct TextureBlitRegion
{
    ImageSubresourceLayers source;
    ImageSubresourceLayers destination;
    Vector3i source_offset = {0, 0, 0};
    /** Zero on an axis means the rest of the source mip level past source_offset on that axis. */
    Vector3i source_extent = {0, 0, 0};
    Vector3i destination_offset = {0, 0, 0};
    /** Zero on an axis means the rest of the destination mip level past destination_offset on that axis. */
    Vector3i destination_extent = {0, 0, 0};
};

/** One box copied from one mip level of a texture into one mip level of another. */
struct TextureCopyRegion
{
    ImageSubresourceLayers source;
    ImageSubresourceLayers destination;
    Vector3i source_offset = {0, 0, 0};
    Vector3i destination_offset = {0, 0, 0};
    /** Zero on an axis means the rest of the source mip level past source_offset on that axis. */
    Vector3i extent = {0, 0, 0};
};

class CommandBuffer
{
public:
    CommandBuffer() = default;
    ~CommandBuffer();

    /**
     * Allocate a primary command buffer out of the queue's command pool.
     *
     * @param device Device the pool belongs to. Has to outlive the command buffer.
     * @param queue Queue the buffer is recorded for and submitted to. Has to outlive it as well.
     * @return The command buffer, or whatever the failing allocation maps to.
     */
    [[nodiscard]] static Opal::Expected<CommandBuffer, ErrorCode> Create(const Device& device, DeviceQueue& queue);

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    CommandBuffer(CommandBuffer&& other) noexcept;
    CommandBuffer& operator=(CommandBuffer&& other) noexcept;

    /** Frees the command buffer and releases associated resources. */
    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_native_command_buffer != VK_NULL_HANDLE; }
    [[nodiscard]] VkCommandBuffer GetNativeCommandBuffer() const { return m_native_command_buffer; }

    /**
     * Begin recording commands into the command buffer.
     * @param submit_one_time If true, the command buffer is intended to be submitted once and then reset or freed.
     * @return ErrorCode::Success, or whatever the failing call maps to.
     */
    [[nodiscard]] ErrorCode Begin(bool submit_one_time = true) const;

    /** End recording commands. Must be called after Begin and before submitting the command buffer. */
    [[nodiscard]] ErrorCode End() const;

    /** Reset the command buffer to its initial state, allowing it to be recorded again. */
    [[nodiscard]] ErrorCode Reset() const;

    /**
     * Insert a pipeline barrier for a single texture. Used to transition image layouts and synchronize access
     * between pipeline stages.
     * @param texture_barrier Describes the source and destination stages, access masks, layouts, and the texture.
     */
    [[nodiscard]] ErrorCode CmdTextureBarrier(const TextureBarrier& texture_barrier);

    /**
     * The same, for a barrier that came from one of the TextureBarrier presets that reads the texture's
     * current layout. A preset that could not be built reports through the command that would have used it,
     * so the two spellings read the same at a call site.
     */
    [[nodiscard]] ErrorCode CmdTextureBarrier(const Opal::Expected<TextureBarrier, ErrorCode>& texture_barrier);

    /**
     * Insert a pipeline barrier for multiple textures in a single call.
     * @param texture_barriers Array of texture barrier descriptions.
     */
    [[nodiscard]] ErrorCode CmdTextureBarriers(Opal::ArrayView<const TextureBarrier> texture_barriers);

    /**
     * Insert a pipeline barrier for a single buffer range. Buffers have no layout, so this only orders access.
     * @param buffer_barrier Describes the source and destination stages, access masks, and the range of the buffer.
     */
    [[nodiscard]] ErrorCode CmdBufferBarrier(const BufferBarrier& buffer_barrier);

    /**
     * Insert a pipeline barrier for multiple buffer ranges in a single call.
     * @param buffer_barriers Array of buffer barrier descriptions.
     */
    [[nodiscard]] ErrorCode CmdBufferBarriers(Opal::ArrayView<const BufferBarrier> buffer_barriers);

    /**
     * Insert a pipeline barrier that covers all memory, without naming a resource.
     * @param memory_barrier Describes the source and destination stages and access masks.
     */
    [[nodiscard]] ErrorCode CmdMemoryBarrier(const MemoryBarrier& memory_barrier);

    /**
     * Insert every barrier of a Barriers group as one dependency. All of the other Cmd*Barrier methods are this
     * one with the other two groups left empty, so batching through it is one pipeline barrier where separate
     * calls would be several.
     * @param barriers Memory, buffer and texture barriers, any of which may be empty.
     */
    [[nodiscard]] ErrorCode CmdBarriers(const Barriers& barriers);

    /**
     * Move a texture into a layout, with the stages and the access picked from what that layout is for. The
     * layout it is coming from is the one the texture tracks, so this is the whole transition in one line.
     * @param texture Texture to transition. Every mip level and array layer of it, so a texture whose levels
     *        disagree - one halfway through mip generation - is refused rather than guessed at.
     * @param new_layout Layout to move it into. One with no barrier preset is refused.
     */
    [[nodiscard]] ErrorCode CmdTransition(Texture& texture, ImageLayout new_layout);

    /**
     * Copy ranges of one buffer into another. The source needs BufferUsageBits::TransferSource and the
     * destination BufferUsageBits::TransferDestination.
     * @param source Buffer to read from.
     * @param destination Buffer to write into.
     * @param regions Ranges to copy. A region reaching past the end of either buffer is refused.
     */
    [[nodiscard]] ErrorCode CmdCopyBuffer(const Buffer& source, const Buffer& destination, Opal::ArrayView<const BufferCopyRegion> regions);

    /** Copy as much of the source as fits in the destination, both from offset zero. */
    [[nodiscard]] ErrorCode CmdCopyBuffer(const Buffer& source, const Buffer& destination);

    /**
     * Copy regions of a buffer into a texture. The buffer needs BufferUsageBits::TransferSource and the
     * texture TextureUsageBits::TransferDestination.
     * @param buffer Source buffer holding the pixels.
     * @param texture Destination texture.
     * @param regions Regions to copy, each naming one mip level of the texture. Every level they name has to
     *        be in TransferDestination or General, which is read off the texture rather than asked for.
     */
    [[nodiscard]] ErrorCode CmdCopyBufferToTexture(const Buffer& buffer, Texture& texture,
                                                   Opal::ArrayView<const BufferTextureCopyRegion> regions);

    /**
     * Copy data from a buffer to a texture. Handles all mip levels described by the bitmap. The destination
     * texture must be in the TransferDestination layout.
     * @param buffer Source buffer containing the pixel data.
     * @param bitmap Bitmap describing the dimensions of the texture and its mip level offsets.
     * @param texture Destination texture to copy into.
     */
    [[nodiscard]] ErrorCode CmdCopyBufferToTexture(const Buffer& buffer, const Bitmap& bitmap, Texture& texture);

    /**
     * Copy regions of a texture into a buffer, which is how anything rendered is read back. The texture needs
     * TextureUsageBits::TransferSource and the buffer BufferUsageBits::TransferDestination.
     * @param texture Source texture.
     * @param buffer Destination buffer.
     * @param regions Regions to copy, each naming one mip level of the texture. Every level they name has to
     *        be in TransferSource or General, which is read off the texture rather than asked for.
     */
    [[nodiscard]] ErrorCode CmdCopyTextureToBuffer(const Texture& texture, const Buffer& buffer,
                                                   Opal::ArrayView<const BufferTextureCopyRegion> regions);

    /**
     * Copy regions of one texture into another. Both must have the same format and sample count, which the
     * validation layer checks; a copy does not convert the way a blit does.
     * @param source Texture to read from. Needs TextureUsageBits::TransferSource.
     * @param destination Texture to write into. Needs TextureUsageBits::TransferDestination.
     * @param regions Regions to copy. The layouts come off the two textures: every level the regions name
     *        has to be in TransferSource on the source and TransferDestination on the destination, or in
     *        General on either.
     */
    [[nodiscard]] ErrorCode CmdCopyTexture(const Texture& source, Texture& destination, Opal::ArrayView<const TextureCopyRegion> regions);

    /**
     * Stretch regions of one texture into another, resampling and converting on the way. The formats need not
     * match, which is what separates this from CmdCopyTexture, but both have to support being blitted - a
     * format that cannot is refused rather than being found out by the validation layer.
     * @param source Texture to read from. Needs TextureUsageBits::TransferSource.
     * @param destination Texture to write into. Needs TextureUsageBits::TransferDestination.
     * @param regions Regions to blit.
     * @param filter How the source is sampled where the two boxes differ in size. A linear filter needs the
     *               source format to support linear filtering.
     * @note The layouts come off the two textures, the way CmdCopyTexture reads them.
     */
    [[nodiscard]] ErrorCode CmdBlitTexture(const Texture& source, Texture& destination, Opal::ArrayView<const TextureBlitRegion> regions,
                                           ImageFilter filter = ImageFilter::Linear);

    /**
     * Fill every mip level below the first by blitting each level into the next, halving it each time. The
     * texture needs both transfer usages and a format this device can blit and filter linearly, all three of
     * which are refused rather than being left to the validation layer.
     * @param texture Texture whose mip 0 is filled and whose remaining levels are not. Every level of it has
     *        to be in the same layout, which is what it is brought out of.
     * @param final_layout Layout to leave the whole texture in, every level in the same one.
     */
    [[nodiscard]] ErrorCode CmdGenerateMips(Texture& texture, ImageLayout final_layout = ImageLayout::ShaderReadOnly);

    /**
     * Begin a dynamic rendering pass. Uses VK_KHR_dynamic_rendering, no render pass or framebuffer objects needed.
     * @param desc Render area, colour attachments, and the optional depth and stencil ones. Each attachment
     *        names a texture, and the layout it is rendered in is read off that texture the way the copies
     *        read theirs - so an attachment whose texture is in a layout its role does not allow is refused,
     *        rather than being a plausible-but-wrong layout no validation layer can catch.
     */
    [[nodiscard]] ErrorCode CmdBeginRendering(const RenderingDesc& desc);

    /** End the current dynamic rendering pass. Must be paired with a prior CmdBeginRendering call. */
    [[nodiscard]] ErrorCode CmdEndRendering();

    /**
     * Set the viewport for subsequent draw commands.
     * @param offset Top-left corner of the viewport in pixels.
     * @param extent Width and height of the viewport in pixels.
     * @param min_depth Minimum depth value of the viewport. Range [0, 1].
     * @param max_depth Maximum depth value of the viewport. Range [0, 1].
     */
    [[nodiscard]] ErrorCode CmdSetViewport(const Vector2f& offset, const Vector2f& extent, f32 min_depth = 0.0f, f32 max_depth = 1.0f);

    /**
     * Set the scissor rectangle for subsequent draw commands. Pixels outside the scissor rectangle are discarded.
     * @param offset Top-left corner of the scissor rectangle in pixels.
     * @param extent Width and height of the scissor rectangle in pixels.
     */
    [[nodiscard]] ErrorCode CmdSetScissor(const Vector2i& offset, const Vector2i& extent);

    /**
     * Set the depth bias for the draws that follow, for a pipeline that named DynamicStateBits::DepthBias.
     * The pipeline still decides whether bias is enabled at all; this only supplies the values.
     * @param constant_factor Added to every fragment, scaled by the smallest depth difference the format resolves.
     * @param clamp Largest bias in either direction. Zero disables clamping, and a non-zero value needs
     *              DeviceFeatures::depth_bias_clamp.
     * @param slope_factor Scaled by how steep the polygon is in screen space.
     */
    [[nodiscard]] ErrorCode CmdSetDepthBias(f32 constant_factor, f32 clamp = 0.0f, f32 slope_factor = 0.0f);

    /**
     * Set the stencil comparison value for the draws that follow, for a pipeline that named
     * DynamicStateBits::StencilReference.
     * @param reference Value the stencil test compares against.
     * @param faces Which faces it applies to.
     */
    [[nodiscard]] ErrorCode CmdSetStencilReference(u32 reference, StencilFaceBits faces = StencilFaceBits::FrontAndBack);

    /**
     * Set which bits the stencil test reads, for a pipeline that named DynamicStateBits::StencilCompareMask.
     * @param compare_mask Bits of the stencil value and of the reference the comparison looks at.
     * @param faces Which faces it applies to.
     */
    [[nodiscard]] ErrorCode CmdSetStencilCompareMask(u32 compare_mask, StencilFaceBits faces = StencilFaceBits::FrontAndBack);

    /**
     * Set which bits a stencil write touches, for a pipeline that named DynamicStateBits::StencilWriteMask.
     * @param write_mask Bits a stencil operation is allowed to change. Zero writes nothing.
     * @param faces Which faces it applies to.
     */
    [[nodiscard]] ErrorCode CmdSetStencilWriteMask(u32 write_mask, StencilFaceBits faces = StencilFaceBits::FrontAndBack);

    /**
     * Set the line width for the draws that follow, for a pipeline that named DynamicStateBits::LineWidth.
     * @param width Width in pixels. Anything other than one needs DeviceFeatures::wide_lines, which this
     *              checks rather than leaving to the validation layer.
     */
    [[nodiscard]] ErrorCode CmdSetLineWidth(f32 width);

    /**
     * Bind a vertex buffer to a specific binding point.
     * @param buffer The vertex buffer to bind.
     * @param binding The binding point index as specified in the vertex input description.
     * @param offset Byte offset into the buffer where vertex data begins.
     */
    [[nodiscard]] ErrorCode CmdBindVertexBuffer(const Buffer& buffer, u32 binding, u64 offset = 0);

    /**
     * Bind an index buffer for subsequent indexed draw commands.
     * @param buffer The index buffer to bind.
     * @param offset Byte offset into the buffer where index data begins.
     * @param index_size Size of each index element (uint8, uint16, or uint32).
     */
    [[nodiscard]] ErrorCode CmdBindIndexBuffer(const Buffer& buffer, u64 offset, IndexSize index_size);

    /**
     * Bind a graphics or compute pipeline. The bind point is determined by the pipeline type.
     * @param pipeline The pipeline to bind.
     */
    [[nodiscard]] ErrorCode CmdBindPipeline(const Pipeline& pipeline);

    /**
     * Bind a single descriptor set to a pipeline.
     * @param pipeline The pipeline whose layout is used for binding.
     * @param descriptor_set The descriptor set to bind.
     * @param first_set Index of the first descriptor set slot to bind to.
     */
    [[nodiscard]] ErrorCode CmdBindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& descriptor_set, u32 first_set = 0);

    /**
     * Bind multiple descriptor sets to a pipeline in a single call.
     * @param pipeline The pipeline whose layout is used for binding.
     * @param descriptor_sets Array of descriptor sets to bind.
     * @param first_set Index of the first descriptor set slot to bind to.
     */
    [[nodiscard]] ErrorCode CmdBindDescriptorSets(const Pipeline& pipeline,
                                                  Opal::ArrayView<const Opal::Ref<const DescriptorSet>> descriptor_sets, u32 first_set = 0);

    /**
     * Push constant data to the pipeline.
     * @param pipeline The pipeline whose layout defines the push constant ranges.
     * @param shader_stages Shader stages that will access the push constant data.
     * @param data Data to push as byte array.
     * @param offset Byte offset into the push constant range.
     */
    [[nodiscard]] ErrorCode CmdPushConstants(const Pipeline& pipeline, ShaderTypeBits shader_stages, Opal::ArrayView<const u8> data,
                                             u32 offset = 0);

    /**
     * Draw indexed primitives.
     * @param index_count Number of indices to draw.
     * @param instance_count Number of instances to draw.
     * @param first_index Offset into the index buffer in number of indices.
     * @param vertex_offset Value added to the vertex index before indexing into the vertex buffer.
     * @param first_instance Instance ID of the first instance to draw.
     */
    [[nodiscard]] ErrorCode CmdDrawIndexed(u32 index_count, u32 instance_count = 1, u32 first_index = 0, i32 vertex_offset = 0,
                                           u32 first_instance = 0);

    /**
     * Draw without an index buffer, walking the vertex buffers in order.
     * @param vertex_count Number of vertices to draw.
     * @param instance_count Number of instances to draw.
     * @param first_vertex Index of the first vertex to draw.
     * @param first_instance Instance ID of the first instance to draw.
     */
    [[nodiscard]] ErrorCode CmdDraw(u32 vertex_count, u32 instance_count = 1, u32 first_vertex = 0, u32 first_instance = 0);

    /**
     * Draw one or more times with the arguments the device reads out of a buffer when it runs the command,
     * rather than from values known while recording.
     * @param buffer Buffer holding the commands. Must have been created with BufferUsageBits::IndirectBuffer.
     * @param offset Byte offset of the first DrawIndirectCommand. Must be a multiple of 4.
     * @param draw_count Number of commands to read.
     * @param stride Bytes between commands. Only read when draw_count is above one.
     */
    [[nodiscard]] ErrorCode CmdDrawIndirect(const Buffer& buffer, u64 offset = 0, u32 draw_count = 1,
                                            u32 stride = static_cast<u32>(sizeof(DrawIndirectCommand)));

    /**
     * The indexed counterpart of CmdDrawIndirect. Reads DrawIndexedIndirectCommand and needs a bound index
     * buffer, the way CmdDrawIndexed does.
     * @param buffer Buffer holding the commands. Must have been created with BufferUsageBits::IndirectBuffer.
     * @param offset Byte offset of the first DrawIndexedIndirectCommand. Must be a multiple of 4.
     * @param draw_count Number of commands to read.
     * @param stride Bytes between commands. Only read when draw_count is above one.
     */
    [[nodiscard]] ErrorCode CmdDrawIndexedIndirect(const Buffer& buffer, u64 offset = 0, u32 draw_count = 1,
                                                   u32 stride = static_cast<u32>(sizeof(DrawIndexedIndirectCommand)));

    /**
     * Draw through the task and mesh shader stages, which replace vertex input and the vertex shader. The
     * counts are in workgroups, the way a compute dispatch counts them.
     * @note Needs the device created with DeviceFeatures::mesh_shader, which pulls in VK_EXT_mesh_shader.
     *       The loader hands out a callable trampoline whether or not the extension was enabled, so this
     *       is refused rather than calling through one that has nothing behind it.
     * @param group_count_x Number of workgroups in the X dimension.
     * @param group_count_y Number of workgroups in the Y dimension.
     * @param group_count_z Number of workgroups in the Z dimension.
     */
    [[nodiscard]] ErrorCode CmdDrawMeshTasks(u32 group_count_x, u32 group_count_y = 1, u32 group_count_z = 1);

    /**
     * Dispatch a compute workload. The counts are in local workgroups, not invocations, so the total invocation
     * count is these multiplied by the local size the compute shader declares. Requires a bound compute pipeline.
     * @param group_count_x Number of local workgroups in the X dimension.
     * @param group_count_y Number of local workgroups in the Y dimension.
     * @param group_count_z Number of local workgroups in the Z dimension.
     */
    [[nodiscard]] ErrorCode CmdDispatch(u32 group_count_x, u32 group_count_y = 1, u32 group_count_z = 1);

    /**
     * Dispatch a compute workload whose group counts the device reads out of a buffer when it runs the command,
     * rather than from values known while recording. The buffer holds a DispatchIndirectCommand at the offset.
     * @param buffer Buffer holding the group counts. Must have been created with BufferUsageBits::IndirectBuffer.
     * @param offset Byte offset of the DispatchIndirectCommand. Must be a multiple of 4.
     */
    [[nodiscard]] ErrorCode CmdDispatchIndirect(const Buffer& buffer, u64 offset = 0);

    /**
     * Open a named region in the command stream, which a capture shows as one collapsible entry in place of
     * the loose commands inside it. Where SetDebugName says what a resource is, a label says what a stretch
     * of work is, so the two answer different questions about the same capture.
     *
     * Every label call is a no-op when the instance did not enable VK_EXT_debug_utils - all three test the
     * same condition, so a build without it skips both halves of a region rather than one. A label is a
     * convenience, and refusing to attach one is never worth failing a frame over.
     *
     * @param name Text shown in place of the region.
     * @param color What a capture tool tints the region with, RGBA in [0, 1]. Purely a hint: Vulkan gives it
     *        no meaning, the validation layer never reads it, and a tool is free to ignore it.
     */
    [[nodiscard]] ErrorCode CmdBeginDebugLabel(const Opal::StringUtf8& name, const Vector4f& color = {1.0f, 1.0f, 1.0f, 1.0f});

    /** Close the region the last CmdBeginDebugLabel opened. Regions nest, and every one has to be closed. */
    [[nodiscard]] ErrorCode CmdEndDebugLabel();

    /**
     * Mark one point in the command stream rather than a region, for something that happens rather than
     * something that takes time.
     * @param name Text shown at the marker.
     * @param color What a capture tool tints the marker with, RGBA in [0, 1]. A hint, as above.
     */
    [[nodiscard]] ErrorCode CmdInsertDebugLabel(const Opal::StringUtf8& name, const Vector4f& color = {1.0f, 1.0f, 1.0f, 1.0f});

    /**
     * Put a range of a query pool back into the state a write needs. A pool holds undefined values until it
     * is reset, so this has to run before the first timestamp is written into it and before every reuse.
     *
     * The host side of this is TimestampQueryPool::Reset, which needs DeviceFeatures::host_query_reset;
     * recording the reset works on every device and is what a per-frame pool wants anyway.
     *
     * @param query_pool Pool to reset.
     * @param first_query First query to reset.
     * @param query_count How many to reset. k_all_queries is the rest of the pool past first_query.
     */
    [[nodiscard]] ErrorCode CmdResetQueryPool(const TimestampQueryPool& query_pool, u32 first_query = 0, u32 query_count = k_all_queries);

    /**
     * Write the GPU tick counter into one query of a pool.
     *
     * This is a marker in the command stream rather than a timer around a scope: the tick is written once
     * every previously submitted command has reached @p stage. Which stage that is decides what a pair of
     * timestamps means - see the timestamp section of docs/forge.md - and the short version is that
     * PipelineStart then PipelineEnd measures a span of the queue while PipelineEnd on both sides measures
     * one operation with the pipeline drained around it.
     *
     * @param query_pool Pool to write into. Must have been reset since the last write to this query.
     * @param query_index Which query of the pool. Past the end of the pool is refused.
     * @param stage Exactly one pipeline stage. More than one bit is refused, since vkCmdWriteTimestamp2 forbids
     *        it, and the stage also has to be one the queue family supports.
     */
    [[nodiscard]] ErrorCode CmdWriteTimestamp(const TimestampQueryPool& query_pool, u32 query_index,
                                              PipelineStageBits stage = PipelineStageBits::AllCommands);

private:
    Opal::Ref<const Device> m_device;
    Opal::Ref<DeviceQueue> m_queue;
    VkCommandBuffer m_native_command_buffer = VK_NULL_HANDLE;
};

}  // namespace Rndr::Forge