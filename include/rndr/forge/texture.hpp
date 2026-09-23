#pragma once

#include "volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/ref.h"

#include "rndr/bitmap.hpp"
#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"

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
    ~Texture();

    /**
     * Create the image and, unless its usage is transfer only, a view of it.
     *
     * @param device Device to allocate from. Has to outlive the texture.
     * @param desc Extent, format, usage and what the view covers.
     * @return The texture, ErrorCode::InvalidArgument when the desc names something it cannot - a cube view
     *         over an array that is not a multiple of six, an enum value that maps to nothing - or whatever
     *         the failing allocation maps to.
     */
    [[nodiscard]] static Opal::Expected<Texture, ErrorCode> Create(const Device& device, const TextureDesc& desc = {});

    /**
     * Upload a bitmap into a new texture, blocking until the copy is done.
     *
     * @param bitmap Source pixels. Its extent, format and mip count are taken over the ones in the desc.
     * @param generate_mips Fill the levels below the first by blitting, for a bitmap that carries only mip 0.
     *                      The full mip chain of the extent is created, and both transfer usages are added.
     * @return The texture, or what the creation, the staging buffer or the upload reported.
     */
    [[nodiscard]] static Opal::Expected<Texture, ErrorCode> Create(const Device& device, DeviceQueue& queue, const Bitmap& bitmap,
                                                                   const TextureDesc& desc = {}, bool generate_mips = false);

    /**
     * Wrap an image Forge did not create - a swap chain one - and give it a view. The image is not owned and
     * is not released with the texture; the view is.
     */
    [[nodiscard]] static Opal::Expected<Texture, ErrorCode> Create(const Device& device, VkImage native_image,
                                                                   const TextureDesc& desc = {});

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
     * The properties of the memory type the image was allocated from. A transient attachment lands in
     * VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT memory when the device has a type the image can use, which is
     * the only way to tell whether it did. Zero for an empty texture and for one wrapping an image Forge
     * did not allocate.
     */
    [[nodiscard]] VkMemoryPropertyFlags GetMemoryProperties() const;

    /**
     * The layout every subresource of the texture is in, which is what a barrier transitions out of. Vulkan
     * keeps no such thing, so this is Forge's own bookkeeping: it starts at ImageLayout::Undefined, the way a
     * freshly created image does, and CommandBuffer moves it on as it records barriers.
     *
     * That makes it a record-time answer rather than an execution-time one - see the barriers section of
     * docs/forge.md for where it stops being true.
     *
     * @return The layout the whole texture is in, or ErrorCode::InvalidArgument when the subresources are
     *         not all in the same one - which is what mip generation leaves behind halfway through. Ask per
     *         subresource instead.
     */
    [[nodiscard]] Opal::Expected<ImageLayout, ErrorCode> GetCurrentLayout() const;

    /**
     * The layout of one subresource.
     * @return The layout, or ErrorCode::OutOfBounds when the texture has no such subresource.
     */
    [[nodiscard]] Opal::Expected<ImageLayout, ErrorCode> GetCurrentLayout(u32 mip_level, u32 array_layer = 0) const;

    /** The layout of a range, answering the way the whole-texture form does when the range is not uniform. */
    [[nodiscard]] Opal::Expected<ImageLayout, ErrorCode> GetCurrentLayout(const ImageSubresourceRange& range) const;

private:
    /**
     * Only CommandBuffer records the barriers that move a layout, and only SwapChain re-acquires an image. A
     * TextureView checks its range the way a barrier does.
     */
    friend class CommandBuffer;
    friend class SwapChain;
    friend class TextureView;

    /** Write a layout over every subresource a range covers, resolving the k_all_* counts against the desc. */
    [[nodiscard]] ErrorCode SetCurrentLayout(const ImageSubresourceRange& range, ImageLayout layout);

    /**
     * Whether a range names only subresources this texture has, which is what SetCurrentLayout checks and what
     * a barrier has to pass before it is recorded rather than after.
     * @return ErrorCode::Success, or ErrorCode::OutOfBounds when the range reaches past the texture.
     */
    [[nodiscard]] ErrorCode CheckRange(const ImageSubresourceRange& range) const;

    [[nodiscard]] ErrorCode Init(const Device& device, const TextureDesc& desc);

    TextureDesc m_desc;
    /** One entry per subresource, mip level major: mip_level * array_layer_count + array_layer. */
    Opal::DynamicArray<ImageLayout> m_layouts;
    Opal::Ref<const Device> m_device;
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VmaAllocation m_image_allocation = VK_NULL_HANDLE;
};

/** What a TextureView sees of its texture: how the image is interpreted, and which part of it. */
struct TextureViewDesc
{
    TextureViewType view_type = TextureViewType::Texture2D;
    /**
     * The mip levels, array layers and aspect the view covers - every one of them by default. A view of a
     * combined depth-stencil format that a shader samples has to name one of the two aspects.
     */
    ImageSubresourceRange subresource_range;
};

/**
 * A second view of a texture's image, apart from the one the texture carries. What a texture needs more than
 * one of: a mip chain written a level at a time as storage images and sampled whole, one layer of an array
 * rendered into while the rest is sampled, one level of a texture rendered into.
 *
 * It does not own the image and keeps no layout of its own. The layout it is used in is the one the texture
 * tracks for the range the view covers, so barriers are still recorded on the texture - with its
 * subresource_range narrowed to this view's - and a view never disagrees with its texture about where the
 * image is. The texture has to outlive every view of it.
 */
class TextureView
{
public:
    TextureView() = default;
    ~TextureView();

    /**
     * @param device Device the texture was created on. Has to outlive the view.
     * @param texture Texture whose image the view is of. Has to outlive the view as well.
     * @param desc The view type and the range.
     * @return The view, ErrorCode::OutOfBounds when the range names mips or layers the texture does not have,
     *         ErrorCode::InvalidArgument for an empty texture, one whose usage allows no view, or a view type
     *         the image cannot take - a cube over a texture not created cube compatible or over a layer count
     *         that is not a multiple of six, a flat view over several layers, a view of another dimension,
     *         a cube array without DeviceFeatures::image_cube_array - or whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<TextureView, ErrorCode> Create(const Device& device, const Texture& texture,
                                                                       const TextureViewDesc& desc = {});

    TextureView(const TextureView&) = delete;
    TextureView& operator=(const TextureView&) = delete;
    TextureView(TextureView&& other) noexcept;
    TextureView& operator=(TextureView&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_view != VK_NULL_HANDLE; }
    [[nodiscard]] VkImageView GetNativeImageView() const { return m_view; }
    [[nodiscard]] const TextureViewDesc& GetDesc() const { return m_desc; }
    /** The texture this is a view of, which is where its layout is tracked. */
    [[nodiscard]] const Texture& GetTexture() const { return *m_texture; }

private:
    Opal::Ref<const Device> m_device;
    Opal::Ref<const Texture> m_texture;
    VkImageView m_view = VK_NULL_HANDLE;
    TextureViewDesc m_desc;
};

class Sampler
{
public:
    Sampler() = default;
    ~Sampler();

    /**
     * @param desc Filters, address modes and the anisotropy and LOD range.
     * @return The sampler, ErrorCode::InvalidArgument when the desc asks for something this device was not
     *         created with - anisotropy, MirrorOnce - or names a value that maps to nothing, or whatever the
     *         failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<Sampler, ErrorCode> Create(const Device& device, const SamplerDesc& desc = {});

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
