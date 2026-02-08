#include "rndr/advanced/advanced-texture.hpp"

#include <vma/vk_mem_alloc.h>

#include "ktxvulkan.h"

#include "rndr/advanced/advanced-buffer.hpp"
#include "rndr/advanced/device.hpp"
#include "rndr/advanced/command-buffer.hpp"
#include "rndr/advanced/synchronization.hpp"
#include "rndr/graphics-types.hpp"

Rndr::AdvancedTexture::AdvancedTexture(const AdvancedDevice& device, const AdvancedTextureDesc& desc) : m_desc(desc), m_device(device)
{
    Init(device, desc);
}

Rndr::AdvancedTexture::AdvancedTexture(const AdvancedDevice& device, AdvancedDeviceQueue& queue, const Bitmap& bitmap,
                                       const AdvancedTextureDesc& desc)
    : m_desc(desc)
{
    m_desc.width = bitmap.GetWidth();
    m_desc.height = bitmap.GetHeight();
    m_desc.depth = bitmap.GetDepth();
    m_desc.mip_level_count = bitmap.GetMipCount();
    m_desc.subresource_range.mip_level_count = bitmap.GetMipCount();
    m_desc.format = bitmap.GetPixelFormat();
    m_desc.image_usage = desc.image_usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // Make sure we always set transfer destination bit

    Init(device, m_desc);

    // Create staging buffer
    const AdvancedBuffer staging_buffer(device, {.size = bitmap.GetTotalSize(), .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT});
    staging_buffer.Update({bitmap.GetData(), bitmap.GetTotalSize()}, 0);

    AdvancedCommandBuffer upload_command_buffer(device, queue);
    upload_command_buffer.Begin();
    upload_command_buffer.CmdImageBarrier({
        .stages_must_finish = PipelineStageBits::None,
        .stages_must_finish_access = PipelineStageAccessBits::None,
        .before_stages_start = PipelineStageBits::Transfer,
        .before_stages_start_access = PipelineStageAccessBits::Write,
        .old_layout = ImageLayout::Undefined,
        .new_layout = ImageLayout::TransferDestination,
        .image = Opal::Ref{this},
        .subresource_range = {.mip_level_count = bitmap.GetMipCount()}
    });
    upload_command_buffer.CmdCopyBufferToImage(staging_buffer, bitmap, *this);
    upload_command_buffer.CmdImageBarrier({
        .stages_must_finish = PipelineStageBits::Transfer,
        .stages_must_finish_access = PipelineStageAccessBits::Write,
        .before_stages_start = PipelineStageBits::FragmentShader,
        .before_stages_start_access = PipelineStageAccessBits::Read,
        .old_layout = ImageLayout::TransferDestination,
        .new_layout = ImageLayout::ShaderReadOnly,
        .image = Opal::Ref{this},
        .subresource_range = {.mip_level_count = bitmap.GetMipCount()}
    });
    upload_command_buffer.End();

    const AdvancedFence fence(device, false);
    queue.Submit(upload_command_buffer, fence);
    fence.Wait();
}

void Rndr::AdvancedTexture::Init(const AdvancedDevice& device, const AdvancedTextureDesc& desc)
{
    m_desc = desc;
    m_device = device;

    VmaAllocator gpu_allocator = device.GetGPUAllocator();

    const VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = m_desc.image_type,
        .format = ToVkFormat(m_desc.format),
        .extent = {.width = m_desc.width, .height = m_desc.height, .depth = m_desc.depth},
        .mipLevels = m_desc.mip_level_count,
        .arrayLayers = m_desc.array_layer_count,
        .samples = m_desc.sample_count,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = m_desc.image_usage,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    const VmaAllocationCreateInfo allocation_create_info = {.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                                            .usage = VMA_MEMORY_USAGE_AUTO};
    VkResult result = vmaCreateImage(gpu_allocator, &image_create_info, &allocation_create_info, &m_image, &m_image_allocation, nullptr);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to allocate the image");
    }
    const VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = m_desc.view_type,
        .format = ToVkFormat(m_desc.format),
        .subresourceRange = {.aspectMask = m_desc.subresource_range.aspect_mask,
                             .baseMipLevel = m_desc.subresource_range.first_mip_level,
                             .levelCount = m_desc.subresource_range.mip_level_count,
                             .baseArrayLayer = m_desc.subresource_range.first_array_layer,
                             .layerCount = m_desc.subresource_range.array_layer_count},
    };
    result = vkCreateImageView(device.GetNativeDevice(), &image_view_create_info, nullptr, &m_view);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create the image view");
    }
}

Rndr::AdvancedTexture::~AdvancedTexture()
{
    Destroy();
}

Rndr::AdvancedTexture::AdvancedTexture(AdvancedTexture&& other) noexcept
    : m_desc(other.m_desc),
      m_device(std::move(other.m_device)),
      m_image(other.m_image),
      m_view(other.m_view),
      m_image_allocation(other.m_image_allocation)
{
    other.m_image = VK_NULL_HANDLE;
    other.m_image_allocation = VK_NULL_HANDLE;
    other.m_view = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::AdvancedTexture& Rndr::AdvancedTexture::operator=(AdvancedTexture&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_device = std::move(other.m_device);
        m_image = other.m_image;
        m_image_allocation = other.m_image_allocation;
        m_view = other.m_view;
        other.m_image = VK_NULL_HANDLE;
        other.m_image_allocation = VK_NULL_HANDLE;
        other.m_view = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::AdvancedTexture::Destroy()
{
    if (m_view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(m_device->GetNativeDevice(), m_view, nullptr);
        m_view = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE)
    {
        vmaDestroyImage(m_device->GetGPUAllocator(), m_image, m_image_allocation);
        m_image = VK_NULL_HANDLE;
        m_image_allocation = VK_NULL_HANDLE;
    }
}
