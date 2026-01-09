#include "rndr/advanced/advanced-texture.hpp"

#include <vma/vk_mem_alloc.h>

#include "rndr/advanced/device.hpp"

Rndr::AdvancedTexture::AdvancedTexture(const class AdvancedDevice& device, const AdvancedTextureDesc& desc) : m_desc(desc), m_device(device)
{
    VmaAllocator gpu_allocator = device.GetGPUAllocator();

    const VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = m_desc.image_type,
        .format = m_desc.format,
        .extent = {.width = desc.width, .height = desc.height, .depth = desc.depth},
        .mipLevels = desc.mip_levels,
        .arrayLayers = desc.array_layers,
        .samples = desc.sample_count,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = desc.image_usage,
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
        .viewType = desc.view_type,
        .format = desc.format,
        .subresourceRange = {.aspectMask = desc.aspect_mask,
                             .baseMipLevel = 0,
                             .levelCount = desc.mip_map_level_count,
                             .baseArrayLayer = 0,
                             .layerCount = desc.array_layers},
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