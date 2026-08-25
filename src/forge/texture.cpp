#include "rndr/forge/texture.hpp"

#include "vk_mem_alloc.h"

#include "rndr/forge/buffer.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/transfer.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/log.hpp"

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

static Opal::Optional<VkSamplerAddressMode> ToVkSamplerAddressMode(Rndr::ImageAddressMode mode)
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
            return {};
    }
}

static Opal::Optional<VkImageType> ToVkImageType(Rndr::Forge::TextureDimension dimension)
{
    switch (dimension)
    {
        case Rndr::Forge::TextureDimension::Texture1D:
            return Opal::Optional<VkImageType>(VK_IMAGE_TYPE_1D);
        case Rndr::Forge::TextureDimension::Texture2D:
            return Opal::Optional<VkImageType>(VK_IMAGE_TYPE_2D);
        case Rndr::Forge::TextureDimension::Texture3D:
            return Opal::Optional<VkImageType>(VK_IMAGE_TYPE_3D);
    }
    return {};
}

static Opal::Optional<VkImageViewType> ToVkImageViewType(Rndr::Forge::TextureViewType view_type)
{
    switch (view_type)
    {
        case Rndr::Forge::TextureViewType::Texture1D:
            return Opal::Optional<VkImageViewType>(VK_IMAGE_VIEW_TYPE_1D);
        case Rndr::Forge::TextureViewType::Texture2D:
            return Opal::Optional<VkImageViewType>(VK_IMAGE_VIEW_TYPE_2D);
        case Rndr::Forge::TextureViewType::Texture3D:
            return Opal::Optional<VkImageViewType>(VK_IMAGE_VIEW_TYPE_3D);
        case Rndr::Forge::TextureViewType::Cube:
            return Opal::Optional<VkImageViewType>(VK_IMAGE_VIEW_TYPE_CUBE);
        case Rndr::Forge::TextureViewType::Texture1DArray:
            return Opal::Optional<VkImageViewType>(VK_IMAGE_VIEW_TYPE_1D_ARRAY);
        case Rndr::Forge::TextureViewType::Texture2DArray:
            return Opal::Optional<VkImageViewType>(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        case Rndr::Forge::TextureViewType::CubeArray:
            return Opal::Optional<VkImageViewType>(VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);
    }
    return {};
}

static Opal::Optional<VkSampleCountFlagBits> ToVkSampleCount(Rndr::Forge::SampleCount sample_count)
{
    switch (sample_count)
    {
        case Rndr::Forge::SampleCount::Count1:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_1_BIT);
        case Rndr::Forge::SampleCount::Count2:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_2_BIT);
        case Rndr::Forge::SampleCount::Count4:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_4_BIT);
        case Rndr::Forge::SampleCount::Count8:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_8_BIT);
        case Rndr::Forge::SampleCount::Count16:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_16_BIT);
        case Rndr::Forge::SampleCount::Count32:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_32_BIT);
        case Rndr::Forge::SampleCount::Count64:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_64_BIT);
    }
    return {};
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

static Opal::Optional<VkBorderColor> ToVkBorderColor(Rndr::BorderColor border_color)
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
            return {};
    }
}

/** How many (mip level, array layer) pairs a texture has, which is how many layouts it tracks. */
static Rndr::i64 SubresourceCount(const Rndr::Forge::TextureDesc& desc)
{
    return static_cast<Rndr::i64>(desc.mip_level_count) * static_cast<Rndr::i64>(desc.array_layer_count);
}

Opal::Expected<Rndr::Forge::Texture, Rndr::ErrorCode> Rndr::Forge::Texture::Create(const Device& device, const TextureDesc& desc)
{
    using Result = Opal::Expected<Texture, ErrorCode>;

    Texture texture;
    const ErrorCode status = texture.Init(device, desc);
    if (status != ErrorCode::Success)
    {
        return Result(status);
    }
    return Result(std::move(texture));
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

Opal::Expected<Rndr::Forge::Texture, Rndr::ErrorCode> Rndr::Forge::Texture::Create(const Device& device, DeviceQueue& queue,
                                                                                   const Bitmap& bitmap, const TextureDesc& desc,
                                                                                   bool generate_mips)
{
    using Result = Opal::Expected<Texture, ErrorCode>;

    TextureDesc image_desc = desc;
    image_desc.width = bitmap.GetWidth();
    image_desc.height = bitmap.GetHeight();
    image_desc.depth = bitmap.GetDepth();
    image_desc.mip_level_count = generate_mips ? FullMipCount(image_desc.width, image_desc.height, image_desc.depth) : bitmap.GetMipCount();
    image_desc.format = bitmap.GetPixelFormat();
    // The upload needs the destination bit either way, and generating the mips reads every level back as well.
    image_desc.usage = desc.usage | TextureUsageBits::TransferDestination;
    if (generate_mips)
    {
        image_desc.usage |= TextureUsageBits::TransferSource;
    }

    // The image and its view are built into this, so an upload that cannot finish leaves through the
    // destructor with both already released.
    Texture texture;
    const ErrorCode init_status = texture.Init(device, image_desc);
    if (init_status != ErrorCode::Success)
    {
        return Result(init_status);
    }

    Opal::Expected<Buffer, ErrorCode> staging_buffer =
        Buffer::Create(device, {.size = bitmap.GetTotalSize(), .usage = BufferUsageBits::TransferSource});
    if (!staging_buffer.HasValue())
    {
        return Result(staging_buffer.GetError());
    }
    const ErrorCode update_status = staging_buffer.GetValue().Update({bitmap.GetData(), bitmap.GetTotalSize()}, 0);
    if (update_status != ErrorCode::Success)
    {
        return Result(update_status);
    }

    // The first code any of the recorded commands reports, which is what the whole upload reports.
    ErrorCode record_status = ErrorCode::Success;
    const ErrorCode submit_status =
        ImmediateSubmit(device, queue,
                        [&](CommandBuffer& command_buffer)
                        {
                            record_status = command_buffer.CmdTextureBarrier(TextureBarrier::ToTransferDestination(texture));
                            if (record_status != ErrorCode::Success)
                            {
                                return;
                            }
                            record_status = command_buffer.CmdCopyBufferToTexture(staging_buffer.GetValue(), bitmap, texture);
                            if (record_status != ErrorCode::Success)
                            {
                                return;
                            }
                            if (generate_mips && image_desc.mip_level_count > 1)
                            {
                                record_status = command_buffer.CmdGenerateMips(texture);
                                return;
                            }
                            record_status = command_buffer.CmdTextureBarrier(TextureBarrier::ToShaderRead(texture));
                        });
    if (record_status != ErrorCode::Success)
    {
        return Result(record_status);
    }
    if (submit_status != ErrorCode::Success)
    {
        return Result(submit_status);
    }
    return Result(std::move(texture));
}

Opal::Expected<Rndr::Forge::Texture, Rndr::ErrorCode> Rndr::Forge::Texture::Create(const Device& device, VkImage native_image,
                                                                                   const TextureDesc& desc)
{
    using Result = Opal::Expected<Texture, ErrorCode>;

    Texture texture;
    texture.m_device = device;
    texture.m_image = native_image;
    texture.m_desc = desc;
    texture.m_layouts = Opal::DynamicArray<ImageLayout>(SubresourceCount(desc), ImageLayout::Undefined);
    RNDR_FORGE_TRANSLATE_EXPECTED(view_type, ToVkImageViewType(desc.view_type), "TextureDesc::view_type", Result);
    const VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = native_image,
        .viewType = view_type,
        .format = ToVkFormat(desc.format),
        .subresourceRange = {.aspectMask = static_cast<VkImageAspectFlags>(desc.subresource_range.ResolveAspectMask(desc.format)),
                             .baseMipLevel = desc.subresource_range.first_mip_level,
                             .levelCount = desc.subresource_range.mip_level_count,
                             .baseArrayLayer = desc.subresource_range.first_array_layer,
                             .layerCount = desc.subresource_range.array_layer_count},
    };
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateImageView(device.GetNativeDevice(), &image_view_create_info, nullptr, &texture.m_view),
                                 "vkCreateImageView", Result);
    return Result(std::move(texture));
}

Rndr::ErrorCode Rndr::Forge::Texture::Init(const Device& device, const TextureDesc& desc)
{
    m_desc = desc;
    m_device = device;
    // Vulkan says a freshly created image is in the undefined layout, whatever initialLayout below asks for,
    // so the grid starts there and the first barrier on the texture transitions out of it.
    m_layouts = Opal::DynamicArray<ImageLayout>(SubresourceCount(m_desc), ImageLayout::Undefined);

    VmaAllocator gpu_allocator = device.GetGPUAllocator();

    // A cube view is only allowed on an image that was created saying one might be taken of it, and there is
    // no way to add the flag afterwards - so the view type the desc already names is what asks for it.
    const bool wants_cube_view = m_desc.view_type == TextureViewType::Cube || m_desc.view_type == TextureViewType::CubeArray;
    if (wants_cube_view && m_desc.array_layer_count % 6 != 0)
    {
        RNDR_LOG_ERROR("Forge: a cube view needs an array layer count that is a multiple of six, got {}", m_desc.array_layer_count);
        return ErrorCode::InvalidArgument;
    }

    RNDR_FORGE_TRANSLATE(image_type, ToVkImageType(m_desc.dimension), "TextureDesc::dimension");
    RNDR_FORGE_TRANSLATE(sample_count, ToVkSampleCount(m_desc.sample_count), "TextureDesc::sample_count");
    const VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = wants_cube_view ? static_cast<VkImageCreateFlags>(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) : 0u,
        .imageType = image_type,
        .format = ToVkFormat(m_desc.format),
        .extent = {.width = m_desc.width, .height = m_desc.height, .depth = m_desc.depth},
        .mipLevels = m_desc.mip_level_count,
        .arrayLayers = m_desc.array_layer_count,
        .samples = sample_count,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        // The values of TextureUsageBits mirror VkImageUsageFlagBits, so the mask translates as a cast.
        .usage = static_cast<VkImageUsageFlags>(m_desc.usage),
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    const VmaAllocationCreateInfo allocation_create_info = {.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                                            .usage = VMA_MEMORY_USAGE_AUTO};
    RNDR_FORGE_VK_CHECK(vmaCreateImage(gpu_allocator, &image_create_info, &allocation_create_info, &m_image, &m_image_allocation, nullptr),
                        "vmaCreateImage");
    // A view is only allowed on an image that some stage can read or write. A texture that is nothing but the
    // source or the destination of a transfer is a legitimate thing to create, and giving it one is invalid.
    if (!SupportsImageView(m_desc.usage))
    {
        return ErrorCode::Success;
    }
    RNDR_FORGE_TRANSLATE(view_type, ToVkImageViewType(m_desc.view_type), "TextureDesc::view_type");
    const VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = view_type,
        .format = ToVkFormat(m_desc.format),
        .subresourceRange = {.aspectMask = static_cast<VkImageAspectFlags>(m_desc.subresource_range.ResolveAspectMask(m_desc.format)),
                             .baseMipLevel = m_desc.subresource_range.first_mip_level,
                             .levelCount = m_desc.subresource_range.mip_level_count,
                             .baseArrayLayer = m_desc.subresource_range.first_array_layer,
                             .layerCount = m_desc.subresource_range.array_layer_count},
    };
    RNDR_FORGE_VK_CHECK(vkCreateImageView(device.GetNativeDevice(), &image_view_create_info, nullptr, &m_view), "vkCreateImageView");
    return ErrorCode::Success;
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
 * that reaches past the texture is refused rather than clamped, since it is a mistake either way.
 */
static Rndr::ErrorCode ResolveRange(const Rndr::Forge::TextureDesc& desc, const Rndr::Forge::ImageSubresourceRange& range,
                                    Rndr::u32& first_mip, Rndr::u32& last_mip, Rndr::u32& first_layer, Rndr::u32& last_layer)
{
    using namespace Rndr;
    first_mip = range.first_mip_level;
    last_mip = range.mip_level_count == Forge::k_all_mip_levels ? desc.mip_level_count : first_mip + range.mip_level_count;
    first_layer = range.first_array_layer;
    last_layer = range.array_layer_count == Forge::k_all_array_layers ? desc.array_layer_count : first_layer + range.array_layer_count;
    if (last_mip > desc.mip_level_count || first_mip >= last_mip)
    {
        RNDR_LOG_ERROR("Forge: the subresource range names mip levels [{}, {}) that a texture with {} does not have", first_mip, last_mip,
                       desc.mip_level_count);
        return ErrorCode::OutOfBounds;
    }
    if (last_layer > desc.array_layer_count || first_layer >= last_layer)
    {
        RNDR_LOG_ERROR("Forge: the subresource range names array layers [{}, {}) that a texture with {} does not have", first_layer,
                       last_layer, desc.array_layer_count);
        return ErrorCode::OutOfBounds;
    }
    return ErrorCode::Success;
}

Opal::Expected<Rndr::Forge::ImageLayout, Rndr::ErrorCode> Rndr::Forge::Texture::GetCurrentLayout() const
{
    return GetCurrentLayout(ImageSubresourceRange{});
}

Opal::Expected<Rndr::Forge::ImageLayout, Rndr::ErrorCode> Rndr::Forge::Texture::GetCurrentLayout(u32 mip_level, u32 array_layer) const
{
    using Result = Opal::Expected<ImageLayout, ErrorCode>;

    if (mip_level >= m_desc.mip_level_count || array_layer >= m_desc.array_layer_count)
    {
        RNDR_LOG_ERROR("Forge: the texture has no subresource at mip {} layer {}", mip_level, array_layer);
        return Result(ErrorCode::OutOfBounds);
    }
    return Result(m_layouts[static_cast<i64>(mip_level) * m_desc.array_layer_count + array_layer]);
}

Opal::Expected<Rndr::Forge::ImageLayout, Rndr::ErrorCode> Rndr::Forge::Texture::GetCurrentLayout(const ImageSubresourceRange& range) const
{
    using Result = Opal::Expected<ImageLayout, ErrorCode>;

    u32 first_mip = 0;
    u32 last_mip = 0;
    u32 first_layer = 0;
    u32 last_layer = 0;
    const ErrorCode range_status = ResolveRange(m_desc, range, first_mip, last_mip, first_layer, last_layer);
    if (range_status != ErrorCode::Success)
    {
        return Result(range_status);
    }
    const ImageLayout common = m_layouts[static_cast<i64>(first_mip) * m_desc.array_layer_count + first_layer];
    for (u32 mip = first_mip; mip < last_mip; ++mip)
    {
        for (u32 layer = first_layer; layer < last_layer; ++layer)
        {
            const ImageLayout layout = m_layouts[static_cast<i64>(mip) * m_desc.array_layer_count + layer];
            if (layout != common)
            {
                RNDR_LOG_ERROR(
                    "Forge: the subresources of the texture are not all in one layout - found {} and {}. Ask for the layout "
                    "of one subresource instead.",
                    ImageLayoutToString(common), ImageLayoutToString(layout));
                return Result(ErrorCode::InvalidArgument);
            }
        }
    }
    return Result(common);
}

Rndr::ErrorCode Rndr::Forge::Texture::SetCurrentLayout(const ImageSubresourceRange& range, ImageLayout layout)
{
    u32 first_mip = 0;
    u32 last_mip = 0;
    u32 first_layer = 0;
    u32 last_layer = 0;
    const ErrorCode range_status = ResolveRange(m_desc, range, first_mip, last_mip, first_layer, last_layer);
    if (range_status != ErrorCode::Success)
    {
        return range_status;
    }
    for (u32 mip = first_mip; mip < last_mip; ++mip)
    {
        for (u32 layer = first_layer; layer < last_layer; ++layer)
        {
            m_layouts[static_cast<i64>(mip) * m_desc.array_layer_count + layer] = layout;
        }
    }
    return ErrorCode::Success;
}

// Sampler

Opal::Expected<Rndr::Forge::Sampler, Rndr::ErrorCode> Rndr::Forge::Sampler::Create(const Device& device, const SamplerDesc& desc)
{
    using Result = Opal::Expected<Sampler, ErrorCode>;

    if (desc.max_anisotropy > 1.0f && !device.GetFeatures().sampler_anisotropy)
    {
        RNDR_LOG_ERROR("Forge: an anisotropic sampler needs the device created with DeviceFeatures::sampler_anisotropy");
        return Result(ErrorCode::InvalidArgument);
    }
    // MIRROR_CLAMP_TO_EDGE is core in Vulkan 1.2 but still a feature, and a sampler naming it on a device
    // that did not enable it is undefined rather than a sampler that fails to create - the layer says so and
    // the driver samples something. Named here, the way max_anisotropy above is.
    const bool mirrors_once = desc.address_mode_u == ImageAddressMode::MirrorOnce || desc.address_mode_v == ImageAddressMode::MirrorOnce ||
                              desc.address_mode_w == ImageAddressMode::MirrorOnce;
    if (mirrors_once && !device.GetFeatures().sampler_mirror_clamp_to_edge)
    {
        RNDR_LOG_ERROR("Forge: ImageAddressMode::MirrorOnce needs the device created with DeviceFeatures::sampler_mirror_clamp_to_edge");
        return Result(ErrorCode::InvalidArgument);
    }
    RNDR_FORGE_TRANSLATE_EXPECTED(address_mode_u, ToVkSamplerAddressMode(desc.address_mode_u), "SamplerDesc::address_mode_u", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(address_mode_v, ToVkSamplerAddressMode(desc.address_mode_v), "SamplerDesc::address_mode_v", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(address_mode_w, ToVkSamplerAddressMode(desc.address_mode_w), "SamplerDesc::address_mode_w", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(border_color, ToVkBorderColor(desc.border_color), "SamplerDesc::border_color", Result);
    Sampler sampler;
    sampler.m_device = device;
    const VkSamplerCreateInfo sampler_create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = ToVkFilter(desc.mag_filter),
        .minFilter = ToVkFilter(desc.min_filter),
        .mipmapMode = ToVkSamplerMipmapMode(desc.mip_map_filter),
        .addressModeU = address_mode_u,
        .addressModeV = address_mode_v,
        .addressModeW = address_mode_w,
        .mipLodBias = desc.lod_bias,
        .anisotropyEnable = desc.max_anisotropy > 1.0f ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = desc.max_anisotropy,
        .minLod = desc.min_lod,
        .maxLod = desc.max_lod,
        .borderColor = border_color,
    };
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateSampler(device.GetNativeDevice(), &sampler_create_info, nullptr, &sampler.m_sampler),
                                 "vkCreateSampler", Result);
    return Result(std::move(sampler));
}

Rndr::Forge::Sampler::~Sampler()
{
    Destroy();
}

Rndr::Forge::Sampler::Sampler(Sampler&& other) noexcept : m_device(std::move(other.m_device)), m_sampler(other.m_sampler)
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
