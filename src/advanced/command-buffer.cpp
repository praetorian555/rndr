#include "rndr/advanced/command-buffer.hpp"

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
    VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier_info
    };
    vkCmdPipelineBarrier2(m_native_command_buffer, &dependency_info);
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
