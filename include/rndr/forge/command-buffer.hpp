#pragma once

#include "volk/volk.h"

#include "opal/container/ref.h"

#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/forward.hpp"
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

struct RenderingAttachmentDesc
{
    VkImageView image_view = VK_NULL_HANDLE;
    /** Undefined by default, so an attachment that was left unconfigured fails loudly instead of rendering wrong. */
    ImageLayout image_layout = ImageLayout::Undefined;
    AttachmentLoadOperation load_operation = AttachmentLoadOperation::Clear;
    AttachmentStoreOperation store_operation = AttachmentStoreOperation::Store;
    union
    {
        Vector4f color;
        struct
        {
            f32 depth;
            u32 stencil;
        } depth_stencil;
    } clear_value = {Vector4f{0.0f, 0.0f, 0.0f, 1.0f}};
};

struct RenderingDesc
{
    Vector2i render_area_extent = {0, 0};
    Opal::DynamicArray<RenderingAttachmentDesc> color_attachments;
    RenderingAttachmentDesc depth_attachment;
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
    Opal::ArrayView<const ImageBarrier> image;
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
struct BufferImageCopyRegion
{
    u64 buffer_offset = 0;
    /** Pixels per row in the buffer. Zero means the rows are packed to the width of image_extent. */
    u32 buffer_row_length = 0;
    /** Rows per array layer in the buffer. Zero means they are packed to the height of image_extent. */
    u32 buffer_image_height = 0;
    ImageSubresourceLayers image_subresource;
    Vector3i image_offset = {0, 0, 0};
    /** Zero on an axis means the rest of the mip level past image_offset on that axis. */
    Vector3i image_extent = {0, 0, 0};
};

/**
 * One box of one mip level of a texture stretched into a box of another. Unlike a copy, the two boxes need
 * not be the same size or the same format - a blit resamples and converts.
 *
 * A negative extent runs the box backwards from its offset, which is how an axis is mirrored.
 */
struct ImageBlitRegion
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
struct ImageCopyRegion
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
    CommandBuffer(const Device& device, DeviceQueue& queue);
    ~CommandBuffer();

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
     */
    void Begin(bool submit_one_time = true) const;

    /** End recording commands. Must be called after Begin and before submitting the command buffer. */
    void End() const;

    /** Reset the command buffer to its initial state, allowing it to be recorded again. */
    void Reset() const;

    /**
     * Insert a pipeline barrier for a single image. Used to transition image layouts and synchronize access between
     * pipeline stages.
     * @param image_barrier Describes the source and destination stages, access masks, layouts, and the target image.
     */
    void CmdImageBarrier(const ImageBarrier& image_barrier);

    /**
     * Insert a pipeline barrier for multiple images in a single call.
     * @param image_barriers Array of image barrier descriptions.
     */
    void CmdImageBarriers(Opal::ArrayView<const ImageBarrier> image_barriers);

    /**
     * Insert a pipeline barrier for a single buffer range. Buffers have no layout, so this only orders access.
     * @param buffer_barrier Describes the source and destination stages, access masks, and the range of the buffer.
     */
    void CmdBufferBarrier(const BufferBarrier& buffer_barrier);

    /**
     * Insert a pipeline barrier for multiple buffer ranges in a single call.
     * @param buffer_barriers Array of buffer barrier descriptions.
     */
    void CmdBufferBarriers(Opal::ArrayView<const BufferBarrier> buffer_barriers);

    /**
     * Insert a pipeline barrier that covers all memory, without naming a resource.
     * @param memory_barrier Describes the source and destination stages and access masks.
     */
    void CmdMemoryBarrier(const MemoryBarrier& memory_barrier);

    /**
     * Insert every barrier of a Barriers group as one dependency. All of the other Cmd*Barrier methods are this
     * one with the other two groups left empty, so batching through it is one pipeline barrier where separate
     * calls would be several.
     * @param barriers Memory, buffer and image barriers, any of which may be empty.
     */
    void CmdBarriers(const Barriers& barriers);

    /**
     * Copy ranges of one buffer into another. The source needs BufferUsageBits::TransferSource and the
     * destination BufferUsageBits::TransferDestination.
     * @param source Buffer to read from.
     * @param destination Buffer to write into.
     * @param regions Ranges to copy. A region reaching past the end of either buffer throws.
     */
    void CmdCopyBuffer(const Buffer& source, const Buffer& destination, Opal::ArrayView<const BufferCopyRegion> regions);

    /** Copy as much of the source as fits in the destination, both from offset zero. */
    void CmdCopyBuffer(const Buffer& source, const Buffer& destination);

    /**
     * Copy regions of a buffer into a texture. The buffer needs BufferUsageBits::TransferSource and the
     * texture TextureUsageBits::TransferDestination.
     * @param buffer Source buffer holding the pixels.
     * @param texture Destination texture.
     * @param regions Regions to copy, each naming one mip level of the texture.
     * @param texture_layout Layout the texture is in. TransferDestination unless the texture is General.
     */
    void CmdCopyBufferToImage(const Buffer& buffer, Texture& texture, Opal::ArrayView<const BufferImageCopyRegion> regions,
                              ImageLayout texture_layout = ImageLayout::TransferDestination);

    /**
     * Copy data from a buffer to an image. Handles all mip levels described by the bitmap. The destination image must
     * be in the TransferDestination layout.
     * @param buffer Source buffer containing the image data.
     * @param bitmap Bitmap describing the image dimensions and mip level offsets.
     * @param texture Destination texture to copy into.
     */
    void CmdCopyBufferToImage(const Buffer& buffer, const Bitmap& bitmap, Texture& texture);

    /**
     * Copy regions of a texture into a buffer, which is how anything rendered is read back. The texture needs
     * TextureUsageBits::TransferSource and the buffer BufferUsageBits::TransferDestination.
     * @param texture Source texture.
     * @param buffer Destination buffer.
     * @param regions Regions to copy, each naming one mip level of the texture.
     * @param texture_layout Layout the texture is in. TransferSource unless the texture is General.
     */
    void CmdCopyImageToBuffer(const Texture& texture, const Buffer& buffer, Opal::ArrayView<const BufferImageCopyRegion> regions,
                              ImageLayout texture_layout = ImageLayout::TransferSource);

    /**
     * Copy regions of one texture into another. Both must have the same format and sample count, which the
     * validation layer checks; a copy does not convert the way a blit does.
     * @param source Texture to read from. Needs TextureUsageBits::TransferSource.
     * @param destination Texture to write into. Needs TextureUsageBits::TransferDestination.
     * @param regions Regions to copy.
     * @param source_layout Layout the source is in.
     * @param destination_layout Layout the destination is in.
     */
    void CmdCopyImage(const Texture& source, Texture& destination, Opal::ArrayView<const ImageCopyRegion> regions,
                      ImageLayout source_layout = ImageLayout::TransferSource,
                      ImageLayout destination_layout = ImageLayout::TransferDestination);

    /**
     * Stretch regions of one texture into another, resampling and converting on the way. The formats need not
     * match, which is what separates this from CmdCopyImage, but both have to support being blitted - a
     * format that cannot throws rather than being found out by the validation layer.
     * @param source Texture to read from. Needs TextureUsageBits::TransferSource.
     * @param destination Texture to write into. Needs TextureUsageBits::TransferDestination.
     * @param regions Regions to blit.
     * @param filter How the source is sampled where the two boxes differ in size. A linear filter needs the
     *               source format to support linear filtering.
     * @param source_layout Layout the source is in.
     * @param destination_layout Layout the destination is in.
     */
    void CmdBlitImage(const Texture& source, Texture& destination, Opal::ArrayView<const ImageBlitRegion> regions,
                      ImageFilter filter = ImageFilter::Linear, ImageLayout source_layout = ImageLayout::TransferSource,
                      ImageLayout destination_layout = ImageLayout::TransferDestination);

    /**
     * Begin a dynamic rendering pass. Uses VK_KHR_dynamic_rendering, no render pass or framebuffer objects needed.
     * @param desc Describes the render area, color attachments, and optional depth attachment.
     */
    void CmdBeginRendering(const RenderingDesc& desc);

    /** End the current dynamic rendering pass. Must be paired with a prior CmdBeginRendering call. */
    void CmdEndRendering();

    /**
     * Set the viewport for subsequent draw commands.
     * @param offset Top-left corner of the viewport in pixels.
     * @param extent Width and height of the viewport in pixels.
     * @param min_depth Minimum depth value of the viewport. Range [0, 1].
     * @param max_depth Maximum depth value of the viewport. Range [0, 1].
     */
    void CmdSetViewport(const Vector2f& offset, const Vector2f& extent, f32 min_depth = 0.0f, f32 max_depth = 1.0f);

    /**
     * Set the scissor rectangle for subsequent draw commands. Pixels outside the scissor rectangle are discarded.
     * @param offset Top-left corner of the scissor rectangle in pixels.
     * @param extent Width and height of the scissor rectangle in pixels.
     */
    void CmdSetScissor(const Vector2i& offset, const Vector2i& extent);

    /**
     * Bind a vertex buffer to a specific binding point.
     * @param buffer The vertex buffer to bind.
     * @param binding The binding point index as specified in the vertex input description.
     * @param offset Byte offset into the buffer where vertex data begins.
     */
    void CmdBindVertexBuffer(const Buffer& buffer, u32 binding, u64 offset = 0);

    /**
     * Bind an index buffer for subsequent indexed draw commands.
     * @param buffer The index buffer to bind.
     * @param offset Byte offset into the buffer where index data begins.
     * @param index_size Size of each index element (uint8, uint16, or uint32).
     */
    void CmdBindIndexBuffer(const Buffer& buffer, u64 offset, IndexSize index_size);

    /**
     * Bind a graphics or compute pipeline. The bind point is determined by the pipeline type.
     * @param pipeline The pipeline to bind.
     */
    void CmdBindPipeline(const Pipeline& pipeline);

    /**
     * Bind a single descriptor set to a pipeline.
     * @param pipeline The pipeline whose layout is used for binding.
     * @param descriptor_set The descriptor set to bind.
     * @param first_set Index of the first descriptor set slot to bind to.
     */
    void CmdBindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& descriptor_set, u32 first_set = 0);

    /**
     * Bind multiple descriptor sets to a pipeline in a single call.
     * @param pipeline The pipeline whose layout is used for binding.
     * @param descriptor_sets Array of descriptor sets to bind.
     * @param first_set Index of the first descriptor set slot to bind to.
     */
    void CmdBindDescriptorSets(const Pipeline& pipeline,
                               Opal::ArrayView<const Opal::Ref<const DescriptorSet>> descriptor_sets, u32 first_set = 0);

    /**
     * Push constant data to the pipeline.
     * @param pipeline The pipeline whose layout defines the push constant ranges.
     * @param shader_stages Shader stages that will access the push constant data.
     * @param data Data to push as byte array.
     * @param offset Byte offset into the push constant range.
     */
    void CmdPushConstants(const Pipeline& pipeline, ShaderTypeBits shader_stages, Opal::ArrayView<const u8> data,
                          u32 offset = 0);

    /**
     * Draw indexed primitives.
     * @param index_count Number of indices to draw.
     * @param instance_count Number of instances to draw.
     * @param first_index Offset into the index buffer in number of indices.
     * @param vertex_offset Value added to the vertex index before indexing into the vertex buffer.
     * @param first_instance Instance ID of the first instance to draw.
     */
    void CmdDrawIndexed(u32 index_count, u32 instance_count = 1, u32 first_index = 0, i32 vertex_offset = 0, u32 first_instance = 0);

    /**
     * Draw without an index buffer, walking the vertex buffers in order.
     * @param vertex_count Number of vertices to draw.
     * @param instance_count Number of instances to draw.
     * @param first_vertex Index of the first vertex to draw.
     * @param first_instance Instance ID of the first instance to draw.
     */
    void CmdDraw(u32 vertex_count, u32 instance_count = 1, u32 first_vertex = 0, u32 first_instance = 0);

    /**
     * Draw one or more times with the arguments the device reads out of a buffer when it runs the command,
     * rather than from values known while recording.
     * @param buffer Buffer holding the commands. Must have been created with BufferUsageBits::IndirectBuffer.
     * @param offset Byte offset of the first DrawIndirectCommand. Must be a multiple of 4.
     * @param draw_count Number of commands to read.
     * @param stride Bytes between commands. Only read when draw_count is above one.
     */
    void CmdDrawIndirect(const Buffer& buffer, u64 offset = 0, u32 draw_count = 1,
                         u32 stride = static_cast<u32>(sizeof(DrawIndirectCommand)));

    /**
     * The indexed counterpart of CmdDrawIndirect. Reads DrawIndexedIndirectCommand and needs a bound index
     * buffer, the way CmdDrawIndexed does.
     * @param buffer Buffer holding the commands. Must have been created with BufferUsageBits::IndirectBuffer.
     * @param offset Byte offset of the first DrawIndexedIndirectCommand. Must be a multiple of 4.
     * @param draw_count Number of commands to read.
     * @param stride Bytes between commands. Only read when draw_count is above one.
     */
    void CmdDrawIndexedIndirect(const Buffer& buffer, u64 offset = 0, u32 draw_count = 1,
                                u32 stride = static_cast<u32>(sizeof(DrawIndexedIndirectCommand)));

    /**
     * Draw through the task and mesh shader stages, which replace vertex input and the vertex shader. The
     * counts are in workgroups, the way a compute dispatch counts them.
     * @note Needs VK_EXT_mesh_shader on the device. Nothing enables it yet - that is device feature chaining -
     *       so this throws rather than calling through a function pointer the loader left null.
     * @param group_count_x Number of workgroups in the X dimension.
     * @param group_count_y Number of workgroups in the Y dimension.
     * @param group_count_z Number of workgroups in the Z dimension.
     */
    void CmdDrawMeshTasks(u32 group_count_x, u32 group_count_y = 1, u32 group_count_z = 1);

    /**
     * Dispatch a compute workload. The counts are in local workgroups, not invocations, so the total invocation
     * count is these multiplied by the local size the compute shader declares. Requires a bound compute pipeline.
     * @param group_count_x Number of local workgroups in the X dimension.
     * @param group_count_y Number of local workgroups in the Y dimension.
     * @param group_count_z Number of local workgroups in the Z dimension.
     */
    void CmdDispatch(u32 group_count_x, u32 group_count_y = 1, u32 group_count_z = 1);

    /**
     * Dispatch a compute workload whose group counts the device reads out of a buffer when it runs the command,
     * rather than from values known while recording. The buffer holds a DispatchIndirectCommand at the offset.
     * @param buffer Buffer holding the group counts. Must have been created with BufferUsageBits::IndirectBuffer.
     * @param offset Byte offset of the DispatchIndirectCommand. Must be a multiple of 4.
     */
    void CmdDispatchIndirect(const Buffer& buffer, u64 offset = 0);

private:
    Opal::Ref<const Device> m_device;
    Opal::Ref<DeviceQueue> m_queue;
    VkCommandBuffer m_native_command_buffer = VK_NULL_HANDLE;
};

}  // namespace Rndr::Forge