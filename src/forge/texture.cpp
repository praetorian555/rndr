#include "rndr/forge/texture.hpp"

#include <vma/vk_mem_alloc.h>

#include "rndr/forge/buffer.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/transfer.hpp"
#include "rndr/forge/vulkan-exception.hpp"
#include "rndr/graphics-types.hpp"

static VkFilter ToVkFilter(Rndr::ImageFilter filter)
{
    switch (filter)
    {
        case Rndr::ImageFilter::Nearest:
            return VK_FILTER_NEAREST;
        case Rndr::ImageFilter::Linear:
            return VK_FILTER_LINEAR;
        default:
            return VK_FILTER_LINEAR;
    }
}

static VkSamplerMipmapMode ToVkSamplerMipmapMode(Rndr::ImageFilter filter)
{
    switch (filter)
    {
        case Rndr::ImageFilter::Nearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case Rndr::ImageFilter::Linear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

static VkSamplerAddressMode ToVkSamplerAddressMode(Rndr::ImageAddressMode mode)
{
    switch (mode)
    {
        case Rndr::ImageAddressMode::Clamp:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case Rndr::ImageAddressMode::Border:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case Rndr::ImageAddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case Rndr::ImageAddressMode::MirrorRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case Rndr::ImageAddressMode::MirrorOnce:
            return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

static VkImageType ToVkImageType(Rndr::Forge::TextureDimension dimension)
{
    switch (dimension)
    {
        case Rndr::Forge::TextureDimension::Texture1D:
            return VK_IMAGE_TYPE_1D;
        case Rndr::Forge::TextureDimension::Texture2D:
            return VK_IMAGE_TYPE_2D;
        case Rndr::Forge::TextureDimension::Texture3D:
            return VK_IMAGE_TYPE_3D;
    }
    throw Opal::Exception("Unknown texture dimension!");
}

static VkImageViewType ToVkImageViewType(Rndr::Forge::TextureViewType view_type)
{
    switch (view_type)
    {
        case Rndr::Forge::TextureViewType::Texture1D:
            return VK_IMAGE_VIEW_TYPE_1D;
        case Rndr::Forge::TextureViewType::Texture2D:
            return VK_IMAGE_VIEW_TYPE_2D;
        case Rndr::Forge::TextureViewType::Texture3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case Rndr::Forge::TextureViewType::Cube:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        case Rndr::Forge::TextureViewType::Texture1DArray:
            return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case Rndr::Forge::TextureViewType::Texture2DArray:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case Rndr::Forge::TextureViewType::CubeArray:
            return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }
    throw Opal::Exception("Unknown texture view type!");
}

static VkSampleCountFlagBits ToVkSampleCount(Rndr::Forge::SampleCount sample_count)
{
    switch (sample_count)
    {
        case Rndr::Forge::SampleCount::Count1:
            return VK_SAMPLE_COUNT_1_BIT;
        case Rndr::Forge::SampleCount::Count2:
            return VK_SAMPLE_COUNT_2_BIT;
        case Rndr::Forge::SampleCount::Count4:
            return VK_SAMPLE_COUNT_4_BIT;
        case Rndr::Forge::SampleCount::Count8:
            return VK_SAMPLE_COUNT_8_BIT;
        case Rndr::Forge::SampleCount::Count16:
            return VK_SAMPLE_COUNT_16_BIT;
        case Rndr::Forge::SampleCount::Count32:
            return VK_SAMPLE_COUNT_32_BIT;
        case Rndr::Forge::SampleCount::Count64:
            return VK_SAMPLE_COUNT_64_BIT;
    }
    throw Opal::Exception("Unknown sample count!");
}

/** Whether any of the usages a view is allowed for is asked for. The transfer usages are not among them. */
static bool SupportsImageView(Rndr::Forge::TextureUsageBits usage)
{
    using Rndr::Forge::TextureUsageBits;
    constexpr TextureUsageBits k_view_usages = TextureUsageBits::Sampled | TextureUsageBits::Storage | TextureUsageBits::ColorAttachment |
                                               TextureUsageBits::DepthStencilAttachment | TextureUsageBits::TransientAttachment |
                                               TextureUsageBits::InputAttachment;
    return !!(usage & k_view_usages);
}

static VkBorderColor ToVkBorderColor(Rndr::BorderColor border_color)
{
    switch (border_color)
    {
        case Rndr::BorderColor::TransparentBlack:
            return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case Rndr::BorderColor::OpaqueBlack:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case Rndr::BorderColor::OpaqueWhite:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        default:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    }
}

/** How many (mip level, array layer) pairs a texture has, which is how many layouts it tracks. */
static Rndr::i64 SubresourceCount(const Rndr::Forge::TextureDesc& desc)
{
    return static_cast<Rndr::i64>(desc.mip_level_count) * static_cast<Rndr::i64>(desc.array_layer_count);
}

Rndr::Forge::Texture::Texture(const Device& device, const TextureDesc& desc) : m_desc(desc), m_device(device)
{
    Init(device, desc);
}

/** How many mip levels an extent has all the way down to one texel on every axis. */
static Rndr::u32 FullMipCount(Rndr::u32 width, Rndr::u32 height, Rndr::u32 depth)
{
    Rndr::u32 largest = Opal::Max(width, Opal::Max(height, depth));
    Rndr::u32 count = 1;
    while (largest > 1)
    {
        largest >>= 1;
        ++count;
    }
    return count;
}

Rndr::Forge::Texture::Texture(const Device& device, DeviceQueue& queue, const Bitmap& bitmap, const TextureDesc& desc,
                              bool generate_mips)
    : m_desc(desc)
{
    m_desc.width = bitmap.GetWidth();
    m_desc.height = bitmap.GetHeight();
    m_desc.depth = bitmap.GetDepth();
    m_desc.mip_level_count = generate_mips ? FullMipCount(m_desc.width, m_desc.height, m_desc.depth) : bitmap.GetMipCount();
    m_desc.format = bitmap.GetPixelFormat();
    // The upload needs the destination bit either way, and generating the mips reads every level back as well.
    m_desc.usage = desc.usage | TextureUsageBits::TransferDestination;
    if (generate_mips)
    {
        m_desc.usage |= TextureUsageBits::TransferSource;
    }

    Init(device, m_desc);

    // The image and its view exist from here on, and the destructor does not run for an object whose
    // constructor threw, so the upload below has to release them itself if it cannot finish.
    try
    {
        // Create staging buffer
        const Buffer staging_buffer(device, {.size = bitmap.GetTotalSize(), .usage = BufferUsageBits::TransferSource});
        staging_buffer.Update({bitmap.GetData(), bitmap.GetTotalSize()}, 0);

        ImmediateSubmit(device, queue,
                        [&](CommandBuffer& command_buffer)
                        {
                            command_buffer.CmdTextureBarrier(TextureBarrier::ToTransferDestination(*this));
                            command_buffer.CmdCopyBufferToTexture(staging_buffer, bitmap, *this);
                            if (generate_mips && m_desc.mip_level_count > 1)
                            {
                                command_buffer.CmdGenerateMips(*this);
                                return;
                            }
                            command_buffer.CmdTextureBarrier(TextureBarrier::ToShaderRead(*this));
                        });
    }
    catch (...)
    {
        Destroy();
        throw;
    }
}

Rndr::Forge::Texture::Texture(const Device& device, VkImage native_image, const TextureDesc& desc) :
m_device(device), m_image(native_image), m_desc(desc)
{
    m_layouts = Opal::DynamicArray<ImageLayout>(SubresourceCount(m_desc), ImageLayout::Undefined);
    const VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = ToVkImageViewType(m_desc.view_type),
        .format = ToVkFormat(m_desc.format),
        .subresourceRange = {.aspectMask = static_cast<VkImageAspectFlags>(m_desc.subresource_range.ResolveAspectMask(m_desc.format)),
                             .baseMipLevel = m_desc.subresource_range.first_mip_level,
                             .levelCount = m_desc.subresource_range.mip_level_count,
                             .baseArrayLayer = m_desc.subresource_range.first_array_layer,
                             .layerCount = m_desc.subresource_range.array_layer_count},
    };
    const VkResult result = vkCreateImageView(device.GetNativeDevice(), &image_view_create_info, nullptr, &m_view);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateImageView");
    }
}

void Rndr::Forge::Texture::Init(const Device& device, const TextureDesc& desc)
{
    m_desc = desc;
    m_device = device;
    // Vulkan says a freshly created image is in the undefined layout, whatever initialLayout below asks for,
    // so the grid starts there and the first barrier on the texture transitions out of it.
    m_layouts = Opal::DynamicArray<ImageLayout>(SubresourceCount(m_desc), ImageLayout::Undefined);

    VmaAllocator gpu_allocator = device.GetGPUAllocator();

    // A cube view is only allowed on an image that was created saying one might be taken of it, and there is
    // no way to add the flag afterwards - so the view type the desc already names is what asks for it.
    const bool wants_cube_view =
        m_desc.view_type == TextureViewType::Cube || m_desc.view_type == TextureViewType::CubeArray;
    if (wants_cube_view && m_desc.array_layer_count % 6 != 0)
    {
        throw Opal::Exception("A cube view needs an array layer count that is a multiple of six!");
    }

    const VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = wants_cube_view ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u,
        .imageType = ToVkImageType(m_desc.dimension),
        .format = ToVkFormat(m_desc.format),
        .extent = {.width = m_desc.width, .height = m_desc.height, .depth = m_desc.depth},
        .mipLevels = m_desc.mip_level_count,
        .arrayLayers = m_desc.array_layer_count,
        .samples = ToVkSampleCount(m_desc.sample_count),
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        // The values of TextureUsageBits mirror VkImageUsageFlagBits, so the mask translates as a cast.
        .usage = static_cast<VkImageUsageFlags>(m_desc.usage),
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    const VmaAllocationCreateInfo allocation_create_info = {.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                                            .usage = VMA_MEMORY_USAGE_AUTO};
    VkResult result = vmaCreateImage(gpu_allocator, &image_create_info, &allocation_create_info, &m_image, &m_image_allocation, nullptr);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vmaCreateImage");
    }
    // A view is only allowed on an image that some stage can read or write. A texture that is nothing but the
    // source or the destination of a transfer is a legitimate thing to create, and giving it one is invalid.
    if (!SupportsImageView(m_desc.usage))
    {
        return;
    }
    const VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = ToVkImageViewType(m_desc.view_type),
        .format = ToVkFormat(m_desc.format),
        .subresourceRange = {.aspectMask = static_cast<VkImageAspectFlags>(m_desc.subresource_range.ResolveAspectMask(m_desc.format)),
                             .baseMipLevel = m_desc.subresource_range.first_mip_level,
                             .levelCount = m_desc.subresource_range.mip_level_count,
                             .baseArrayLayer = m_desc.subresource_range.first_array_layer,
                             .layerCount = m_desc.subresource_range.array_layer_count},
    };
    result = vkCreateImageView(device.GetNativeDevice(), &image_view_create_info, nullptr, &m_view);
    if (result != VK_SUCCESS)
    {
        // The destructor does not run for an object whose constructor threw, so the image above would leak.
        Destroy();
        throw VulkanException(result, "vkCreateImageView");
    }
}

Rndr::Forge::Texture::~Texture()
{
    Destroy();
}

Rndr::Forge::Texture::Texture(Texture&& other) noexcept
    : m_desc(other.m_desc),
      m_layouts(std::move(other.m_layouts)),
      m_device(std::move(other.m_device)),
      m_image(other.m_image),
      m_view(other.m_view),
      m_image_allocation(other.m_image_allocation)
{
    other.m_layouts.Clear();
    other.m_image = VK_NULL_HANDLE;
    other.m_image_allocation = VK_NULL_HANDLE;
    other.m_view = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::Forge::Texture& Rndr::Forge::Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_layouts = std::move(other.m_layouts);
        m_device = std::move(other.m_device);
        m_image = other.m_image;
        m_image_allocation = other.m_image_allocation;
        m_view = other.m_view;
        other.m_layouts.Clear();
        other.m_image = VK_NULL_HANDLE;
        other.m_image_allocation = VK_NULL_HANDLE;
        other.m_view = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::Forge::Texture::Destroy()
{
    if (m_view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(m_device->GetNativeDevice(), m_view, nullptr);
        m_view = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE && m_image_allocation != VK_NULL_HANDLE)
    {
        vmaDestroyImage(m_device->GetGPUAllocator(), m_image, m_image_allocation);
        m_image = VK_NULL_HANDLE;
        m_image_allocation = VK_NULL_HANDLE;
    }
    else
    {
        // We were not the owner of the native image
        m_image = VK_NULL_HANDLE;
    }
    m_layouts.Clear();
}

/**
 * The half-open subresource range a range names, with the k_all_* counts resolved against the desc. A range
 * that reaches past the texture throws rather than being clamped, since it is a mistake either way.
 */
static void ResolveRange(const Rndr::Forge::TextureDesc& desc, const Rndr::Forge::ImageSubresourceRange& range,
                         Rndr::u32& first_mip, Rndr::u32& last_mip, Rndr::u32& first_layer, Rndr::u32& last_layer)
{
    using namespace Rndr;
    first_mip = range.first_mip_level;
    last_mip = range.mip_level_count == Forge::k_all_mip_levels ? desc.mip_level_count : first_mip + range.mip_level_count;
    first_layer = range.first_array_layer;
    last_layer = range.array_layer_count == Forge::k_all_array_layers ? desc.array_layer_count : first_layer + range.array_layer_count;
    if (last_mip > desc.mip_level_count || first_mip >= last_mip)
    {
        throw Opal::Exception("The subresource range names mip levels the texture does not have!");
    }
    if (last_layer > desc.array_layer_count || first_layer >= last_layer)
    {
        throw Opal::Exception("The subresource range names array layers the texture does not have!");
    }
}

Rndr::Forge::ImageLayout Rndr::Forge::Texture::GetCurrentLayout() const
{
    return GetCurrentLayout(ImageSubresourceRange{});
}

Rndr::Forge::ImageLayout Rndr::Forge::Texture::GetCurrentLayout(u32 mip_level, u32 array_layer) const
{
    if (mip_level >= m_desc.mip_level_count || array_layer >= m_desc.array_layer_count)
    {
        throw Opal::Exception("The texture does not have that subresource!");
    }
    return m_layouts[static_cast<i64>(mip_level) * m_desc.array_layer_count + array_layer];
}

Rndr::Forge::ImageLayout Rndr::Forge::Texture::GetCurrentLayout(const ImageSubresourceRange& range) const
{
    u32 first_mip = 0;
    u32 last_mip = 0;
    u32 first_layer = 0;
    u32 last_layer = 0;
    ResolveRange(m_desc, range, first_mip, last_mip, first_layer, last_layer);
    const ImageLayout common = m_layouts[static_cast<i64>(first_mip) * m_desc.array_layer_count + first_layer];
    for (u32 mip = first_mip; mip < last_mip; ++mip)
    {
        for (u32 layer = first_layer; layer < last_layer; ++layer)
        {
            const ImageLayout layout = m_layouts[static_cast<i64>(mip) * m_desc.array_layer_count + layer];
            if (layout != common)
            {
                throw Opal::Exception(Opal::StringEx("The subresources of the texture are not all in one layout - found ") +
                                      ImageLayoutToString(common) + " and " + ImageLayoutToString(layout) +
                                      ". Ask for the layout of one subresource instead.");
            }
        }
    }
    return common;
}

void Rndr::Forge::Texture::SetCurrentLayout(const ImageSubresourceRange& range, ImageLayout layout)
{
    u32 first_mip = 0;
    u32 last_mip = 0;
    u32 first_layer = 0;
    u32 last_layer = 0;
    ResolveRange(m_desc, range, first_mip, last_mip, first_layer, last_layer);
    for (u32 mip = first_mip; mip < last_mip; ++mip)
    {
        for (u32 layer = first_layer; layer < last_layer; ++layer)
        {
            m_layouts[static_cast<i64>(mip) * m_desc.array_layer_count + layer] = layout;
        }
    }
}

// Sampler

Rndr::Forge::Sampler::Sampler(const Device& device, const SamplerDesc& desc) : m_device(device)
{
    if (desc.max_anisotropy > 1.0f && !device.GetFeatures().sampler_anisotropy)
    {
        throw Opal::Exception("An anisotropic sampler needs the device created with DeviceFeatures::sampler_anisotropy.");
    }
    const VkSamplerCreateInfo sampler_create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = ToVkFilter(desc.mag_filter),
        .minFilter = ToVkFilter(desc.min_filter),
        .mipmapMode = ToVkSamplerMipmapMode(desc.mip_map_filter),
        .addressModeU = ToVkSamplerAddressMode(desc.address_mode_u),
        .addressModeV = ToVkSamplerAddressMode(desc.address_mode_v),
        .addressModeW = ToVkSamplerAddressMode(desc.address_mode_w),
        .mipLodBias = desc.lod_bias,
        .anisotropyEnable = desc.max_anisotropy > 1.0f ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = desc.max_anisotropy,
        .minLod = desc.min_lod,
        .maxLod = desc.max_lod,
        .borderColor = ToVkBorderColor(desc.border_color),
    };
    const VkResult result = vkCreateSampler(device.GetNativeDevice(), &sampler_create_info, nullptr, &m_sampler);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateSampler");
    }
}

Rndr::Forge::Sampler::~Sampler()
{
    Destroy();
}

Rndr::Forge::Sampler::Sampler(Sampler&& other) noexcept
    : m_device(std::move(other.m_device)), m_sampler(other.m_sampler)
{
    other.m_sampler = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::Forge::Sampler& Rndr::Forge::Sampler::operator=(Sampler&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_sampler = other.m_sampler;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::Forge::Sampler::Destroy()
{
    if (m_sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_device->GetNativeDevice(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
}
