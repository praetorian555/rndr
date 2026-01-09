#pragma once

#include "vma/vk_mem_alloc.h"
#include "volk/volk.h"

#include "opal/container/ref.h"

#include "rndr/types.hpp"

namespace Rndr
{

struct AdvancedTextureDesc
{
    // Image
    VkImageType image_type = VK_IMAGE_TYPE_2D;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    u32 width = 0;
    u32 height = 0;
    u32 depth = 1;
    u32 mip_levels = 1;
    u32 array_layers = 1;
    VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
    VkImageUsageFlags image_usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    // Image view
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
    u32 mip_map_level_count = 1;  // Number of mip map levels to include
    u32 layer_count = 1;          // Number of array layers to include
};

class AdvancedTexture
{
public:
    AdvancedTexture() = default;
    explicit AdvancedTexture(const class AdvancedDevice& device, const AdvancedTextureDesc& desc = {});
    ~AdvancedTexture();

    AdvancedTexture(const AdvancedTexture&) = delete;
    AdvancedTexture& operator=(const AdvancedTexture&) = delete;
    AdvancedTexture(AdvancedTexture&& other) noexcept;
    AdvancedTexture& operator=(AdvancedTexture&& other) noexcept;

    void Destroy();

    [[nodiscard]] VkImage GetNativeImage() const { return m_image; }
    [[nodiscard]] VkImageView GetNativeImageView() const { return m_view; }

private:
    AdvancedTextureDesc m_desc;
    Opal::Ref<const class AdvancedDevice> m_device;
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VmaAllocation m_image_allocation;
};

}  // namespace Rndr
