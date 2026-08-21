#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"
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
    /**
     * Upload a bitmap into a new texture, blocking until the copy is done.
     * @param bitmap Source pixels. Its extent, format and mip count are taken over the ones in the desc.
     * @param generate_mips Fill the levels below the first by blitting, for a bitmap that carries only mip 0.
     *                      The full mip chain of the extent is created, and both transfer usages are added.
     */
    explicit Texture(const Device& device, DeviceQueue& queue, const Bitmap& bitmap, const TextureDesc& desc = {},
                     bool generate_mips = false);
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

    /**
     * The layout every subresource of the texture is in, which is what a barrier transitions out of. Vulkan
     * keeps no such thing, so this is Forge's own bookkeeping: it starts at ImageLayout::Undefined, the way a
     * freshly created image does, and CommandBuffer moves it on as it records barriers.
     *
     * That makes it a record-time answer rather than an execution-time one - see the barriers section of
     * docs/forge.md for where it stops being true.
     *
     * @return The layout the whole texture is in. Throws when the subresources are not all in the same one,
     *         which is what mip generation leaves behind halfway through; ask per subresource instead.
     */
    [[nodiscard]] ImageLayout GetCurrentLayout() const;

    /** The layout of one subresource. */
    [[nodiscard]] ImageLayout GetCurrentLayout(u32 mip_level, u32 array_layer = 0) const;

    /** The layout of a range, throwing the way the whole-texture form does when the range is not uniform. */
    [[nodiscard]] ImageLayout GetCurrentLayout(const ImageSubresourceRange& range) const;

private:
    /** Only CommandBuffer records the barriers that move a layout, and only SwapChain re-acquires an image. */
    friend class CommandBuffer;
    friend class SwapChain;

    /** Write a layout over every subresource a range covers, resolving the k_all_* counts against the desc. */
    void SetCurrentLayout(const ImageSubresourceRange& range, ImageLayout layout);

    void Init(const Device& device, const TextureDesc& desc);

    TextureDesc m_desc;
    /** One entry per subresource, mip level major: mip_level * array_layer_count + array_layer. */
    Opal::DynamicArray<ImageLayout> m_layouts;
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
