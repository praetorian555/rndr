#pragma once

#include "volk/volk.h"

#include "opal/container/ref.h"

#include "rndr/bitmap.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"

// Forward declare handle to avoid vma includes in headers.
using VmaAllocation = struct VmaAllocation_T*;

namespace Rndr::Forge
{

struct TextureDesc
{
    // Image
    TextureDimension dimension = TextureDimension::Texture2D;
    PixelFormat format = PixelFormat::B8G8R8A8_UNORM;
    u32 width = 0;
    u32 height = 0;
    u32 depth = 1;
    u32 mip_level_count = 1;
    u32 array_layer_count = 1;
    SampleCount sample_count = SampleCount::Count1;
    TextureUsageBits usage = TextureUsageBits::Sampled;

    // Image view
    TextureViewType view_type = TextureViewType::Texture2D;
    ImageSubresourceRange subresource_range;
};

class Texture
{
public:
    Texture() = default;
    explicit Texture(const Device& device, const TextureDesc& desc = {});
    explicit Texture(const Device& device, DeviceQueue& queue, const Bitmap& bitmap,
                             const TextureDesc& desc = {});
    explicit Texture(const Device& device, VkImage native_image, const TextureDesc& desc = {});
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_image != VK_NULL_HANDLE; }
    [[nodiscard]] VkImage GetNativeImage() const { return m_image; }
    /** Null for a texture whose usage is transfer only, since Vulkan allows no view on one. */
    [[nodiscard]] VkImageView GetNativeImageView() const { return m_view; }
    [[nodiscard]] const TextureDesc& GetDesc() const { return m_desc; }

private:
    void Init(const Device& device, const TextureDesc& desc);

    TextureDesc m_desc;
    Opal::Ref<const Device> m_device;
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VmaAllocation m_image_allocation = VK_NULL_HANDLE;
};

class Sampler
{
public:
    Sampler() = default;
    explicit Sampler(const Device& device, const SamplerDesc& desc = {});
    ~Sampler();

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&& other) noexcept;
    Sampler& operator=(Sampler&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_sampler != VK_NULL_HANDLE; }
    [[nodiscard]] VkSampler GetNativeSampler() const { return m_sampler; }

private:
    Opal::Ref<const Device> m_device;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

}  // namespace Rndr::Forge
