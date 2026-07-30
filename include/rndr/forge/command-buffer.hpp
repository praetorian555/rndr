#pragma once

#include "volk/volk.h"

#include "opal/container/ref.h"

#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/forward.hpp"

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
    void CmdImageBarriers(const Opal::ArrayView<ImageBarrier>& image_barriers);

    /**
     * Copy data from a buffer to an image. Handles all mip levels described by the bitmap. The destination image must
     * be in the TransferDestination layout.
     * @param buffer Source buffer containing the image data.
     * @param bitmap Bitmap describing the image dimensions and mip level offsets.
     * @param texture Destination texture to copy into.
     */
    void CmdCopyBufferToImage(const Buffer& buffer, const Bitmap& bitmap, Texture& texture);

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
                               const Opal::ArrayView<Opal::Ref<DescriptorSet>>& descriptor_sets, u32 first_set = 0);

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

private:
    Opal::Ref<const Device> m_device;
    Opal::Ref<DeviceQueue> m_queue;
    VkCommandBuffer m_native_command_buffer = VK_NULL_HANDLE;
};

}  // namespace Rndr::Forge