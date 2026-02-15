#include "rndr/advanced/command-buffer.hpp"

#include "rndr/advanced/advanced-buffer.hpp"
#include "rndr/advanced/advanced-descriptor-set.hpp"
#include "rndr/advanced/advanced-pipeline.hpp"
#include "rndr/advanced/device.hpp"

Rndr::AdvancedCommandBuffer::AdvancedCommandBuffer(const AdvancedDevice& device, AdvancedDeviceQueue& queue)
    : m_device(&device), m_queue(&queue)
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_queue->GetNativeCommandPool();
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    const VkResult result = vkAllocateCommandBuffers(m_device->GetNativeDevice(), &alloc_info, &m_native_command_buffer);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to allocate command buffer!");
    }
}

Rndr::AdvancedCommandBuffer::~AdvancedCommandBuffer()
{
    Destroy();
}

void Rndr::AdvancedCommandBuffer::Destroy()
{
    if (m_native_command_buffer != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(m_device->GetNativeDevice(), m_queue->GetNativeCommandPool(), 1, &m_native_command_buffer);
        m_native_command_buffer = VK_NULL_HANDLE;
    }
}

void Rndr::AdvancedCommandBuffer::Begin(bool submit_one_time) const
{
    VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0};
    begin_info.flags |= submit_one_time ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;

    const VkResult result = vkBeginCommandBuffer(m_native_command_buffer, &begin_info);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to begin command buffer!");
    }
}

void Rndr::AdvancedCommandBuffer::End() const
{
    if (vkEndCommandBuffer(m_native_command_buffer) != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to end command buffer!");
    }
}

void Rndr::AdvancedCommandBuffer::Reset() const
{
    if (vkResetCommandBuffer(m_native_command_buffer, 0) != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to reset command buffer!");
    }
}

void Rndr::AdvancedCommandBuffer::CmdImageBarrier(const AdvancedImageBarrier& image_barrier)
{
    VkImageMemoryBarrier2 barrier_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = static_cast<VkPipelineStageFlags2>(image_barrier.stages_must_finish),
        .srcAccessMask = static_cast<VkAccessFlags2>(image_barrier.stages_must_finish_access),
        .dstStageMask = static_cast<VkPipelineStageFlags2>(image_barrier.before_stages_start),
        .dstAccessMask = static_cast<VkAccessFlags2>(image_barrier.before_stages_start_access),
        .oldLayout = static_cast<VkImageLayout>(image_barrier.old_layout),
        .newLayout = static_cast<VkImageLayout>(image_barrier.new_layout),
        .image = image_barrier.image.Get().GetNativeImage(),
    };
    barrier_info.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(image_barrier.subresource_range.aspect_mask);
    barrier_info.subresourceRange.baseMipLevel = image_barrier.subresource_range.first_mip_level;
    barrier_info.subresourceRange.levelCount = image_barrier.subresource_range.mip_level_count;
    barrier_info.subresourceRange.baseArrayLayer = image_barrier.subresource_range.first_array_layer;
    barrier_info.subresourceRange.layerCount = image_barrier.subresource_range.array_layer_count;
    const VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier_info};
    vkCmdPipelineBarrier2(m_native_command_buffer, &dependency_info);
}

void Rndr::AdvancedCommandBuffer::CmdImageBarriers(const Opal::ArrayView<AdvancedImageBarrier>& image_barriers)
{
    Opal::DynamicArray<VkImageMemoryBarrier2> barriers(Opal::GetScratchAllocator());
    for (const auto& image_barrier : image_barriers)
    {
        VkImageMemoryBarrier2 barrier_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = static_cast<VkPipelineStageFlags2>(image_barrier.stages_must_finish),
            .srcAccessMask = static_cast<VkAccessFlags2>(image_barrier.stages_must_finish_access),
            .dstStageMask = static_cast<VkPipelineStageFlags2>(image_barrier.before_stages_start),
            .dstAccessMask = static_cast<VkAccessFlags2>(image_barrier.before_stages_start_access),
            .oldLayout = static_cast<VkImageLayout>(image_barrier.old_layout),
            .newLayout = static_cast<VkImageLayout>(image_barrier.new_layout),
            .image = image_barrier.image.Get().GetNativeImage(),
        };
        barrier_info.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(image_barrier.subresource_range.aspect_mask);
        barrier_info.subresourceRange.baseMipLevel = image_barrier.subresource_range.first_mip_level;
        barrier_info.subresourceRange.levelCount = image_barrier.subresource_range.mip_level_count;
        barrier_info.subresourceRange.baseArrayLayer = image_barrier.subresource_range.first_array_layer;
        barrier_info.subresourceRange.layerCount = image_barrier.subresource_range.array_layer_count;
        barriers.PushBack(barrier_info);
    }
    const VkDependencyInfo dependency_info{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                           .imageMemoryBarrierCount = static_cast<u32>(barriers.GetSize()),
                                           .pImageMemoryBarriers = barriers.GetData()};
    vkCmdPipelineBarrier2(m_native_command_buffer, &dependency_info);
}

void Rndr::AdvancedCommandBuffer::CmdCopyBufferToImage(const class AdvancedBuffer& buffer, const Bitmap& bitmap, AdvancedTexture& texture)
{
    Opal::DynamicArray<VkBufferImageCopy> copy_regions(bitmap.GetMipCount(), Opal::GetScratchAllocator());
    for (u32 mip_level = 0; mip_level < bitmap.GetMipCount(); ++mip_level)
    {
        const u32 width = Opal::Max(1, bitmap.GetWidth() >> mip_level);
        const u32 height = Opal::Max(1, bitmap.GetHeight() >> mip_level);
        const u32 depth = Opal::Max(1, bitmap.GetDepth() >> mip_level);
        const VkBufferImageCopy copy_region{
            .bufferOffset = bitmap.GetMipLevelOffset(static_cast<u32>(mip_level)),
            .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = mip_level, .layerCount = 1},
            .imageExtent = {.width = width, .height = height, .depth = depth},
        };
        copy_regions[mip_level] = copy_region;
    }
    vkCmdCopyBufferToImage(m_native_command_buffer, buffer.GetNativeBuffer(), texture.GetNativeImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<u32>(copy_regions.GetSize()), copy_regions.GetData());
}

static VkAttachmentLoadOp ToVkLoadOp(Rndr::AttachmentLoadOperation op)
{
    switch (op)
    {
        case Rndr::AttachmentLoadOperation::Load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case Rndr::AttachmentLoadOperation::Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case Rndr::AttachmentLoadOperation::DontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default:
            throw Opal::Exception("Unsupported attachment load operation");
    }
}

static VkAttachmentStoreOp ToVkStoreOp(Rndr::AttachmentStoreOperation op)
{
    switch (op)
    {
        case Rndr::AttachmentStoreOperation::Store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case Rndr::AttachmentStoreOperation::DontCare:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default:
            throw Opal::Exception("Unsupported attachment store operation");
    }
}

void Rndr::AdvancedCommandBuffer::CmdBeginRendering(const AdvancedRenderingDesc& desc)
{
    Opal::DynamicArray<VkRenderingAttachmentInfo> color_attachments(Opal::GetScratchAllocator());
    for (const auto& attachment : desc.color_attachments)
    {
        color_attachments.PushBack(VkRenderingAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = attachment.image_view,
            .imageLayout = static_cast<VkImageLayout>(attachment.image_layout),
            .loadOp = ToVkLoadOp(attachment.load_operation),
            .storeOp = ToVkStoreOp(attachment.store_operation),
            .clearValue = {.color = {.float32 = {attachment.clear_value.color.r, attachment.clear_value.color.g,
                                                 attachment.clear_value.color.b, attachment.clear_value.color.a}}},
        });
    }

    const VkRenderingAttachmentInfo depth_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = desc.depth_attachment.image_view,
        .imageLayout = static_cast<VkImageLayout>(desc.depth_attachment.image_layout),
        .loadOp = ToVkLoadOp(desc.depth_attachment.load_operation),
        .storeOp = ToVkStoreOp(desc.depth_attachment.store_operation),
        .clearValue = {.depthStencil = {.depth = desc.depth_attachment.clear_value.depth_stencil.depth,
                                        .stencil = desc.depth_attachment.clear_value.depth_stencil.stencil}},
    };

    const bool has_depth = desc.depth_attachment.image_view != VK_NULL_HANDLE;
    const VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {.extent = {.width = static_cast<u32>(desc.render_area_extent.x),
                                  .height = static_cast<u32>(desc.render_area_extent.y)}},
        .layerCount = 1,
        .colorAttachmentCount = static_cast<u32>(color_attachments.GetSize()),
        .pColorAttachments = color_attachments.GetData(),
        .pDepthAttachment = has_depth ? &depth_attachment : nullptr,
    };
    vkCmdBeginRendering(m_native_command_buffer, &rendering_info);
}

void Rndr::AdvancedCommandBuffer::CmdEndRendering()
{
    vkCmdEndRendering(m_native_command_buffer);
}

void Rndr::AdvancedCommandBuffer::CmdSetViewport(const Vector2f& offset, const Vector2f& extent, f32 min_depth, f32 max_depth)
{
    const VkViewport viewport{
        .x = offset.x, .y = offset.y, .width = extent.x, .height = extent.y, .minDepth = min_depth, .maxDepth = max_depth};
    vkCmdSetViewport(m_native_command_buffer, 0, 1, &viewport);
}

void Rndr::AdvancedCommandBuffer::CmdSetScissor(const Vector2i& offset, const Vector2i& extent)
{
    const VkRect2D scissor{.offset = {.x = offset.x, .y = offset.y},
                           .extent = {.width = static_cast<u32>(extent.x), .height = static_cast<u32>(extent.y)}};
    vkCmdSetScissor(m_native_command_buffer, 0, 1, &scissor);
}

void Rndr::AdvancedCommandBuffer::CmdBindVertexBuffer(const AdvancedBuffer& buffer, u32 binding, u64 offset)
{
    const VkBuffer native_buffer = buffer.GetNativeBuffer();
    vkCmdBindVertexBuffers(m_native_command_buffer, binding, 1, &native_buffer, &offset);
}

static VkIndexType ToVkIndexType(Rndr::IndexSize index_size)
{
    switch (index_size)
    {
        case Rndr::IndexSize::uint8:
            return VK_INDEX_TYPE_UINT8_KHR;
        case Rndr::IndexSize::uint16:
            return VK_INDEX_TYPE_UINT16;
        case Rndr::IndexSize::uint32:
            return VK_INDEX_TYPE_UINT32;
        default:
            throw Opal::Exception("Unsupported index size");
    }
}

void Rndr::AdvancedCommandBuffer::CmdBindIndexBuffer(const AdvancedBuffer& buffer, u64 offset, IndexSize index_size)
{
    vkCmdBindIndexBuffer(m_native_command_buffer, buffer.GetNativeBuffer(), offset, ToVkIndexType(index_size));
}

static VkShaderStageFlags ToVkShaderStageFlags(Rndr::ShaderTypeBits stages)
{
    VkShaderStageFlags flags = 0;
    if (!!(stages & Rndr::ShaderTypeBits::AllGraphics))
    {
        flags |= VK_SHADER_STAGE_ALL_GRAPHICS;
    }
    if (!!(stages & Rndr::ShaderTypeBits::Vertex))
    {
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if (!!(stages & Rndr::ShaderTypeBits::Fragment))
    {
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    if (!!(stages & Rndr::ShaderTypeBits::Compute))
    {
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    }
    if (!!(stages & Rndr::ShaderTypeBits::Task))
    {
        flags |= VK_SHADER_STAGE_TASK_BIT_EXT;
    }
    if (!!(stages & Rndr::ShaderTypeBits::Mesh))
    {
        flags |= VK_SHADER_STAGE_MESH_BIT_EXT;
    }
    return flags;
}

void Rndr::AdvancedCommandBuffer::CmdBindPipeline(const AdvancedPipeline& pipeline)
{
    vkCmdBindPipeline(m_native_command_buffer, pipeline.GetBindPoint(), pipeline.GetNativePipeline());
}

void Rndr::AdvancedCommandBuffer::CmdBindDescriptorSet(const AdvancedPipeline& pipeline, const AdvancedDescriptorSet& descriptor_set,
                                                       u32 first_set)
{
    const VkDescriptorSet native_set = descriptor_set.GetNativeDescriptorSet();
    vkCmdBindDescriptorSets(m_native_command_buffer, pipeline.GetBindPoint(), pipeline.GetNativePipelineLayout(), first_set, 1, &native_set,
                            0, nullptr);
}

void Rndr::AdvancedCommandBuffer::CmdBindDescriptorSets(const AdvancedPipeline& pipeline,
                                                        const Opal::ArrayView<Opal::Ref<AdvancedDescriptorSet>>& descriptor_sets,
                                                        u32 first_set)
{
    Opal::DynamicArray<VkDescriptorSet> native_sets(Opal::GetScratchAllocator());
    for (const auto& set : descriptor_sets)
    {
        native_sets.PushBack(set->GetNativeDescriptorSet());
    }
    vkCmdBindDescriptorSets(m_native_command_buffer, pipeline.GetBindPoint(), pipeline.GetNativePipelineLayout(), first_set,
                            static_cast<u32>(native_sets.GetSize()), native_sets.GetData(), 0, nullptr);
}

void Rndr::AdvancedCommandBuffer::CmdPushConstants(const AdvancedPipeline& pipeline, ShaderTypeBits shader_stages,
                                                   Opal::ArrayView<const u8> data, u32 offset)
{
    vkCmdPushConstants(m_native_command_buffer, pipeline.GetNativePipelineLayout(), ToVkShaderStageFlags(shader_stages), offset,
                       static_cast<u32>(data.GetSize()), data.GetData());
}

void Rndr::AdvancedCommandBuffer::CmdDrawIndexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset,
                                                 u32 first_instance)
{
    vkCmdDrawIndexed(m_native_command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

Rndr::AdvancedCommandBuffer::AdvancedCommandBuffer(AdvancedCommandBuffer&& other) noexcept
    : m_device(std::move(other.m_device)), m_queue(std::move(other.m_queue)), m_native_command_buffer(other.m_native_command_buffer)
{
    other.m_native_command_buffer = VK_NULL_HANDLE;
}

Rndr::AdvancedCommandBuffer& Rndr::AdvancedCommandBuffer::operator=(AdvancedCommandBuffer&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_queue = std::move(other.m_queue);
        m_native_command_buffer = other.m_native_command_buffer;
        other.m_native_command_buffer = VK_NULL_HANDLE;
    }
    return *this;
}
