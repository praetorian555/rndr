#include "rndr/forge/command-buffer.hpp"

#include "rndr/forge/buffer.hpp"
#include "rndr/forge/descriptor-set.hpp"
#include "rndr/forge/pipeline.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-exception.hpp"

Rndr::Forge::CommandBuffer::CommandBuffer(const Device& device, DeviceQueue& queue)
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
        throw VulkanException(result, "vkAllocateCommandBuffers");
    }
}

Rndr::Forge::CommandBuffer::~CommandBuffer()
{
    Destroy();
}

void Rndr::Forge::CommandBuffer::Destroy()
{
    if (m_native_command_buffer != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(m_device->GetNativeDevice(), m_queue->GetNativeCommandPool(), 1, &m_native_command_buffer);
        m_native_command_buffer = VK_NULL_HANDLE;
    }
}

void Rndr::Forge::CommandBuffer::Begin(bool submit_one_time) const
{
    VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0};
    begin_info.flags |= submit_one_time ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;

    const VkResult result = vkBeginCommandBuffer(m_native_command_buffer, &begin_info);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkBeginCommandBuffer");
    }
}

void Rndr::Forge::CommandBuffer::End() const
{
    const VkResult end_result = vkEndCommandBuffer(m_native_command_buffer);
    if (end_result != VK_SUCCESS)
    {
        throw VulkanException(end_result, "vkEndCommandBuffer");
    }
}

void Rndr::Forge::CommandBuffer::Reset() const
{
    const VkResult reset_result = vkResetCommandBuffer(m_native_command_buffer, 0);
    if (reset_result != VK_SUCCESS)
    {
        throw VulkanException(reset_result, "vkResetCommandBuffer");
    }
}

static VkImageMemoryBarrier2 ToVkImageBarrier(const Rndr::Forge::ImageBarrier& image_barrier)
{
    const Rndr::Forge::Texture& texture = image_barrier.image.Get();
    const Rndr::Forge::ImageSubresourceRange& range = image_barrier.subresource_range;
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = static_cast<VkPipelineStageFlags2>(image_barrier.stages_must_finish),
        .srcAccessMask = static_cast<VkAccessFlags2>(image_barrier.stages_must_finish_access),
        .dstStageMask = static_cast<VkPipelineStageFlags2>(image_barrier.before_stages_start),
        .dstAccessMask = static_cast<VkAccessFlags2>(image_barrier.before_stages_start_access),
        .oldLayout = static_cast<VkImageLayout>(image_barrier.old_layout),
        .newLayout = static_cast<VkImageLayout>(image_barrier.new_layout),
        // Forge does not transfer ownership between queue families yet. Leaving these zero would name family
        // zero, which a barrier on an image created with VK_SHARING_MODE_CONCURRENT is not allowed to do.
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture.GetNativeImage(),
        .subresourceRange = {.aspectMask = static_cast<VkImageAspectFlags>(range.ResolveAspectMask(texture.GetDesc().format)),
                             .baseMipLevel = range.first_mip_level,
                             .levelCount = range.mip_level_count,
                             .baseArrayLayer = range.first_array_layer,
                             .layerCount = range.array_layer_count},
    };
}

static VkBufferMemoryBarrier2 ToVkBufferBarrier(const Rndr::Forge::BufferBarrier& buffer_barrier)
{
    return {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = static_cast<VkPipelineStageFlags2>(buffer_barrier.stages_must_finish),
        .srcAccessMask = static_cast<VkAccessFlags2>(buffer_barrier.stages_must_finish_access),
        .dstStageMask = static_cast<VkPipelineStageFlags2>(buffer_barrier.before_stages_start),
        .dstAccessMask = static_cast<VkAccessFlags2>(buffer_barrier.before_stages_start_access),
        // See ToVkImageBarrier - Forge does not transfer ownership between queue families yet.
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buffer_barrier.buffer.Get().GetNativeBuffer(),
        .offset = buffer_barrier.offset,
        .size = buffer_barrier.size,
    };
}

static VkMemoryBarrier2 ToVkMemoryBarrier(const Rndr::Forge::MemoryBarrier& memory_barrier)
{
    return {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = static_cast<VkPipelineStageFlags2>(memory_barrier.stages_must_finish),
        .srcAccessMask = static_cast<VkAccessFlags2>(memory_barrier.stages_must_finish_access),
        .dstStageMask = static_cast<VkPipelineStageFlags2>(memory_barrier.before_stages_start),
        .dstAccessMask = static_cast<VkAccessFlags2>(memory_barrier.before_stages_start_access),
    };
}

void Rndr::Forge::CommandBuffer::CmdBarriers(const Barriers& barriers)
{
    Opal::DynamicArray<VkMemoryBarrier2> memory_barriers(barriers.memory.GetSize());
    for (i32 i = 0; i < barriers.memory.GetSize(); ++i)
    {
        memory_barriers[i] = ToVkMemoryBarrier(barriers.memory[i]);
    }
    Opal::DynamicArray<VkBufferMemoryBarrier2> buffer_barriers(barriers.buffer.GetSize());
    for (i32 i = 0; i < barriers.buffer.GetSize(); ++i)
    {
        buffer_barriers[i] = ToVkBufferBarrier(barriers.buffer[i]);
    }
    Opal::DynamicArray<VkImageMemoryBarrier2> image_barriers(barriers.image.GetSize());
    for (i32 i = 0; i < barriers.image.GetSize(); ++i)
    {
        image_barriers[i] = ToVkImageBarrier(barriers.image[i]);
    }
    if (memory_barriers.IsEmpty() && buffer_barriers.IsEmpty() && image_barriers.IsEmpty())
    {
        return;
    }
    const VkDependencyInfo dependency_info{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                           .memoryBarrierCount = static_cast<u32>(memory_barriers.GetSize()),
                                           .pMemoryBarriers = memory_barriers.GetData(),
                                           .bufferMemoryBarrierCount = static_cast<u32>(buffer_barriers.GetSize()),
                                           .pBufferMemoryBarriers = buffer_barriers.GetData(),
                                           .imageMemoryBarrierCount = static_cast<u32>(image_barriers.GetSize()),
                                           .pImageMemoryBarriers = image_barriers.GetData()};
    vkCmdPipelineBarrier2(m_native_command_buffer, &dependency_info);
}

void Rndr::Forge::CommandBuffer::CmdImageBarrier(const ImageBarrier& image_barrier)
{
    CmdBarriers({.image = {&image_barrier, 1}});
}

void Rndr::Forge::CommandBuffer::CmdImageBarriers(Opal::ArrayView<const ImageBarrier> image_barriers)
{
    CmdBarriers({.image = image_barriers});
}

void Rndr::Forge::CommandBuffer::CmdBufferBarrier(const BufferBarrier& buffer_barrier)
{
    CmdBarriers({.buffer = {&buffer_barrier, 1}});
}

void Rndr::Forge::CommandBuffer::CmdBufferBarriers(Opal::ArrayView<const BufferBarrier> buffer_barriers)
{
    CmdBarriers({.buffer = buffer_barriers});
}

void Rndr::Forge::CommandBuffer::CmdMemoryBarrier(const MemoryBarrier& memory_barrier)
{
    CmdBarriers({.memory = {&memory_barrier, 1}});
}

/** The extent of one mip level of a texture, which is the base extent halved once per level, floored at one. */
static Rndr::u32 MipExtent(Rndr::u32 base_extent, Rndr::u32 mip_level)
{
    return Opal::Max(1u, base_extent >> mip_level);
}

/** The buffer has to allow the transfer it is about to take part in. */
static void ValidateBufferUsage(const Rndr::Forge::Buffer& buffer, Rndr::Forge::BufferUsageBits required, const char* role,
                                const char* what)
{
    if (!(buffer.GetDesc().usage & required))
    {
        throw Opal::Exception(Opal::StringEx(what) + " needs a " + role + " buffer created with the matching BufferUsageBits!");
    }
}

/** The texture has to allow the transfer it is about to take part in. */
static void ValidateTextureUsage(const Rndr::Forge::Texture& texture, Rndr::Forge::TextureUsageBits required, const char* role,
                                 const char* what)
{
    if (!(texture.GetDesc().usage & required))
    {
        throw Opal::Exception(Opal::StringEx(what) + " needs a " + role + " texture created with the matching TextureUsageBits!");
    }
}

/** The range has to fit, written so that neither a large offset nor a large size can overflow the sum and pass. */
static void ValidateBufferRange(const Rndr::Forge::Buffer& buffer, Rndr::u64 offset, Rndr::u64 size, const char* role, const char* what)
{
    const Rndr::u64 buffer_size = buffer.GetSize();
    if (offset > buffer_size || size > buffer_size - offset)
    {
        throw Opal::Exception(Opal::StringEx(what) + " reaches past the end of the " + role + " buffer!");
    }
}

/**
 * Check that the subresource exists on the texture and report the extent of the mip level it names, which is
 * what every region on that texture is measured against.
 */
static VkExtent3D ValidateSubresource(const Rndr::Forge::Texture& texture, const Rndr::Forge::ImageSubresourceLayers& subresource,
                                      const char* role, const char* what)
{
    using namespace Rndr;
    const Forge::TextureDesc& desc = texture.GetDesc();
    if (subresource.mip_level >= desc.mip_level_count)
    {
        throw Opal::Exception(Opal::StringEx(what) + " names a mip level the " + role + " texture does not have!");
    }
    if (subresource.array_layer_count == 0 || subresource.first_array_layer > desc.array_layer_count ||
        subresource.array_layer_count > desc.array_layer_count - subresource.first_array_layer)
    {
        throw Opal::Exception(Opal::StringEx(what) + " names array layers the " + role + " texture does not have!");
    }
    return {.width = MipExtent(desc.width, subresource.mip_level),
            .height = MipExtent(desc.height, subresource.mip_level),
            .depth = MipExtent(desc.depth, subresource.mip_level)};
}

/**
 * Resolve one image side of a copy: check that the subresource exists on the texture, fill a zero extent in
 * with the rest of the mip level, and check that the box fits inside it.
 */
static VkExtent3D ResolveImageRegion(const Rndr::Forge::Texture& texture, const Rndr::Forge::ImageSubresourceLayers& subresource,
                                     const Rndr::Vector3i& offset, const Rndr::Vector3i& extent, const char* role, const char* what)
{
    using namespace Rndr;
    const VkExtent3D mip_extent = ValidateSubresource(texture, subresource, role, what);
    if (offset.x < 0 || offset.y < 0 || offset.z < 0 || extent.x < 0 || extent.y < 0 || extent.z < 0)
    {
        throw Opal::Exception(Opal::StringEx(what) + " has a negative offset or extent on the " + role + " texture!");
    }
    const u32 offset_x = static_cast<u32>(offset.x);
    const u32 offset_y = static_cast<u32>(offset.y);
    const u32 offset_z = static_cast<u32>(offset.z);
    if (offset_x > mip_extent.width || offset_y > mip_extent.height || offset_z > mip_extent.depth)
    {
        throw Opal::Exception(Opal::StringEx(what) + " starts outside the mip level of the " + role + " texture!");
    }
    // A zero extent on an axis means the rest of the mip level past the offset on that axis.
    const VkExtent3D resolved{.width = extent.x != 0 ? static_cast<u32>(extent.x) : mip_extent.width - offset_x,
                              .height = extent.y != 0 ? static_cast<u32>(extent.y) : mip_extent.height - offset_y,
                              .depth = extent.z != 0 ? static_cast<u32>(extent.z) : mip_extent.depth - offset_z};
    if (resolved.width > mip_extent.width - offset_x || resolved.height > mip_extent.height - offset_y ||
        resolved.depth > mip_extent.depth - offset_z)
    {
        throw Opal::Exception(Opal::StringEx(what) + " reaches past the mip level of the " + role + " texture!");
    }
    return resolved;
}

/**
 * Resolve one side of a blit into the two corners Vulkan wants. A blit names corners rather than an extent so
 * that it can run an axis backwards, which is how it mirrors, so a negative extent is allowed here and the
 * far corner can sit before the near one.
 */
static void ResolveBlitBox(const Rndr::Forge::Texture& texture, const Rndr::Forge::ImageSubresourceLayers& subresource,
                           const Rndr::Vector3i& offset, const Rndr::Vector3i& extent, const char* role, const char* what,
                           VkOffset3D corners[2])
{
    using namespace Rndr;
    const VkExtent3D mip_extent = ValidateSubresource(texture, subresource, role, what);
    const i32 limits[3] = {static_cast<i32>(mip_extent.width), static_cast<i32>(mip_extent.height), static_cast<i32>(mip_extent.depth)};
    const i32 near_corner[3] = {offset.x, offset.y, offset.z};
    const i32 sizes[3] = {extent.x, extent.y, extent.z};
    i32 far_corner[3] = {};
    for (i32 axis = 0; axis < 3; ++axis)
    {
        // A zero extent means the rest of the mip level, which is the whole axis when the offset is zero.
        far_corner[axis] = sizes[axis] != 0 ? near_corner[axis] + sizes[axis] : limits[axis];
        if (near_corner[axis] < 0 || near_corner[axis] > limits[axis] || far_corner[axis] < 0 || far_corner[axis] > limits[axis])
        {
            throw Opal::Exception(Opal::StringEx(what) + " reaches past the mip level of the " + role + " texture!");
        }
        if (near_corner[axis] == far_corner[axis])
        {
            throw Opal::Exception(Opal::StringEx(what) + " has an empty box on the " + role + " texture!");
        }
    }
    corners[0] = {.x = near_corner[0], .y = near_corner[1], .z = near_corner[2]};
    corners[1] = {.x = far_corner[0], .y = far_corner[1], .z = far_corner[2]};
}

static VkImageSubresourceLayers ToVkSubresourceLayers(const Rndr::Forge::ImageSubresourceLayers& subresource, Rndr::PixelFormat format)
{
    return {.aspectMask = static_cast<VkImageAspectFlags>(Rndr::Forge::ResolveAspectMask(subresource.aspect_mask, format)),
            .mipLevel = subresource.mip_level,
            .baseArrayLayer = subresource.first_array_layer,
            .layerCount = subresource.array_layer_count};
}

/**
 * One region of a copy between a buffer and a texture, translated and checked from both ends. Both directions
 * describe the copy the same way, so they share this.
 */
static VkBufferImageCopy ToVkBufferImageCopy(const Rndr::Forge::Buffer& buffer, const Rndr::Forge::Texture& texture,
                                             const Rndr::Forge::BufferImageCopyRegion& region, const char* buffer_role, const char* what)
{
    using namespace Rndr;
    const VkExtent3D extent = ResolveImageRegion(texture, region.image_subresource, region.image_offset, region.image_extent, "target",
                                                 what);
    if (region.buffer_offset % 4 != 0)
    {
        throw Opal::Exception(Opal::StringEx(what) + " buffer offset must be a multiple of 4!");
    }
    // A compressed format is measured in blocks rather than pixels, so the size below would be wrong for one.
    // GetPixelSize reports zero for those, and the validation layer covers what this cannot.
    const u32 pixel_size = GetPixelSize(texture.GetDesc().format);
    if (pixel_size != 0)
    {
        const u64 row_length = region.buffer_row_length != 0 ? region.buffer_row_length : extent.width;
        const u64 image_height = region.buffer_image_height != 0 ? region.buffer_image_height : extent.height;
        const u64 region_size = static_cast<u64>(pixel_size) * row_length * image_height * extent.depth *
                                region.image_subresource.array_layer_count;
        ValidateBufferRange(buffer, region.buffer_offset, region_size, buffer_role, what);
    }
    return {.bufferOffset = region.buffer_offset,
            .bufferRowLength = region.buffer_row_length,
            .bufferImageHeight = region.buffer_image_height,
            .imageSubresource = ToVkSubresourceLayers(region.image_subresource, texture.GetDesc().format),
            .imageOffset = {.x = region.image_offset.x, .y = region.image_offset.y, .z = region.image_offset.z},
            .imageExtent = extent};
}

void Rndr::Forge::CommandBuffer::CmdCopyBuffer(const Buffer& source, const Buffer& destination,
                                               Opal::ArrayView<const BufferCopyRegion> regions)
{
    if (regions.IsEmpty())
    {
        return;
    }
    ValidateBufferUsage(source, BufferUsageBits::TransferSource, "source", "Buffer copy");
    ValidateBufferUsage(destination, BufferUsageBits::TransferDestination, "destination", "Buffer copy");
    Opal::DynamicArray<VkBufferCopy> copy_regions(regions.GetSize());
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        const BufferCopyRegion& region = regions[i];
        if (region.source_offset > source.GetSize() || region.destination_offset > destination.GetSize())
        {
            throw Opal::Exception("Buffer copy starts past the end of a buffer!");
        }
        // The whole buffer means as much as both sides have left, so that neither end is overrun.
        const u64 size = region.size == k_whole_buffer ? Opal::Min(source.GetSize() - region.source_offset,
                                                                  destination.GetSize() - region.destination_offset)
                                                       : region.size;
        ValidateBufferRange(source, region.source_offset, size, "source", "Buffer copy");
        ValidateBufferRange(destination, region.destination_offset, size, "destination", "Buffer copy");
        copy_regions[i] = {.srcOffset = region.source_offset, .dstOffset = region.destination_offset, .size = size};
    }
    vkCmdCopyBuffer(m_native_command_buffer, source.GetNativeBuffer(), destination.GetNativeBuffer(),
                    static_cast<u32>(copy_regions.GetSize()), copy_regions.GetData());
}

void Rndr::Forge::CommandBuffer::CmdCopyBuffer(const Buffer& source, const Buffer& destination)
{
    const BufferCopyRegion region;
    CmdCopyBuffer(source, destination, {&region, 1});
}

void Rndr::Forge::CommandBuffer::CmdCopyBufferToImage(const Buffer& buffer, Texture& texture,
                                                      Opal::ArrayView<const BufferImageCopyRegion> regions, ImageLayout texture_layout)
{
    if (regions.IsEmpty())
    {
        return;
    }
    ValidateBufferUsage(buffer, BufferUsageBits::TransferSource, "source", "Buffer to image copy");
    ValidateTextureUsage(texture, TextureUsageBits::TransferDestination, "destination", "Buffer to image copy");
    Opal::DynamicArray<VkBufferImageCopy> copy_regions(regions.GetSize());
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        copy_regions[i] = ToVkBufferImageCopy(buffer, texture, regions[i], "source", "Buffer to image copy");
    }
    vkCmdCopyBufferToImage(m_native_command_buffer, buffer.GetNativeBuffer(), texture.GetNativeImage(),
                           static_cast<VkImageLayout>(texture_layout), static_cast<u32>(copy_regions.GetSize()), copy_regions.GetData());
}

void Rndr::Forge::CommandBuffer::CmdCopyBufferToImage(const Buffer& buffer, const Bitmap& bitmap, Texture& texture)
{
    // One region per mip level, laid out the way the bitmap packs them. The aspect and the extent come from the
    // texture through the general path below, so this no longer assumes a color format.
    Opal::DynamicArray<BufferImageCopyRegion> regions(bitmap.GetMipCount());
    for (u32 mip_level = 0; mip_level < bitmap.GetMipCount(); ++mip_level)
    {
        regions[mip_level] = {.buffer_offset = bitmap.GetMipLevelOffset(static_cast<i32>(mip_level)),
                              .image_subresource = {.mip_level = mip_level}};
    }
    CmdCopyBufferToImage(buffer, texture, regions);
}

void Rndr::Forge::CommandBuffer::CmdCopyImageToBuffer(const Texture& texture, const Buffer& buffer,
                                                      Opal::ArrayView<const BufferImageCopyRegion> regions, ImageLayout texture_layout)
{
    if (regions.IsEmpty())
    {
        return;
    }
    ValidateTextureUsage(texture, TextureUsageBits::TransferSource, "source", "Image to buffer copy");
    ValidateBufferUsage(buffer, BufferUsageBits::TransferDestination, "destination", "Image to buffer copy");
    Opal::DynamicArray<VkBufferImageCopy> copy_regions(regions.GetSize());
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        copy_regions[i] = ToVkBufferImageCopy(buffer, texture, regions[i], "destination", "Image to buffer copy");
    }
    vkCmdCopyImageToBuffer(m_native_command_buffer, texture.GetNativeImage(), static_cast<VkImageLayout>(texture_layout),
                           buffer.GetNativeBuffer(), static_cast<u32>(copy_regions.GetSize()), copy_regions.GetData());
}

void Rndr::Forge::CommandBuffer::CmdCopyImage(const Texture& source, Texture& destination, Opal::ArrayView<const ImageCopyRegion> regions,
                                              ImageLayout source_layout, ImageLayout destination_layout)
{
    if (regions.IsEmpty())
    {
        return;
    }
    ValidateTextureUsage(source, TextureUsageBits::TransferSource, "source", "Image copy");
    ValidateTextureUsage(destination, TextureUsageBits::TransferDestination, "destination", "Image copy");
    Opal::DynamicArray<VkImageCopy> copy_regions(regions.GetSize());
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        const ImageCopyRegion& region = regions[i];
        // The extent is resolved against the source and then checked against the destination, so a zero extent
        // means the rest of the source mip level and still has to fit where it is going.
        const VkExtent3D extent = ResolveImageRegion(source, region.source, region.source_offset, region.extent, "source", "Image copy");
        const Vector3i resolved_extent{static_cast<i32>(extent.width), static_cast<i32>(extent.height), static_cast<i32>(extent.depth)};
        ResolveImageRegion(destination, region.destination, region.destination_offset, resolved_extent, "destination", "Image copy");
        copy_regions[i] = {
            .srcSubresource = ToVkSubresourceLayers(region.source, source.GetDesc().format),
            .srcOffset = {.x = region.source_offset.x, .y = region.source_offset.y, .z = region.source_offset.z},
            .dstSubresource = ToVkSubresourceLayers(region.destination, destination.GetDesc().format),
            .dstOffset = {.x = region.destination_offset.x, .y = region.destination_offset.y, .z = region.destination_offset.z},
            .extent = extent};
    }
    vkCmdCopyImage(m_native_command_buffer, source.GetNativeImage(), static_cast<VkImageLayout>(source_layout),
                   destination.GetNativeImage(), static_cast<VkImageLayout>(destination_layout),
                   static_cast<u32>(copy_regions.GetSize()), copy_regions.GetData());
}

static VkFilter ToVkFilter(Rndr::ImageFilter filter)
{
    switch (filter)
    {
        case Rndr::ImageFilter::Nearest:
            return VK_FILTER_NEAREST;
        case Rndr::ImageFilter::Linear:
            return VK_FILTER_LINEAR;
        default:
            throw Opal::Exception("Unsupported image filter");
    }
}

void Rndr::Forge::CommandBuffer::CmdBlitImage(const Texture& source, Texture& destination, Opal::ArrayView<const ImageBlitRegion> regions,
                                              ImageFilter filter, ImageLayout source_layout, ImageLayout destination_layout)
{
    if (regions.IsEmpty())
    {
        return;
    }
    ValidateTextureUsage(source, TextureUsageBits::TransferSource, "source", "Image blit");
    ValidateTextureUsage(destination, TextureUsageBits::TransferDestination, "destination", "Image blit");
    // Blitting is per format and per side, and a format that cannot be blitted is a driver-specific surprise
    // rather than a mistake in the calling code, so it is worth naming here instead of at the validation layer.
    const PhysicalDevice& physical_device = m_device->GetPhysicalDevice();
    if (!physical_device.SupportsBlit(source.GetDesc().format, true))
    {
        throw Opal::Exception("This device cannot blit from the format of the source texture!");
    }
    if (!physical_device.SupportsBlit(destination.GetDesc().format, false))
    {
        throw Opal::Exception("This device cannot blit into the format of the destination texture!");
    }
    if (filter == ImageFilter::Linear && !physical_device.SupportsLinearFilter(source.GetDesc().format))
    {
        throw Opal::Exception("This device cannot filter the format of the source texture linearly!");
    }
    Opal::DynamicArray<VkImageBlit> blit_regions(regions.GetSize());
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        const ImageBlitRegion& region = regions[i];
        VkImageBlit blit{.srcSubresource = ToVkSubresourceLayers(region.source, source.GetDesc().format),
                         .dstSubresource = ToVkSubresourceLayers(region.destination, destination.GetDesc().format)};
        ResolveBlitBox(source, region.source, region.source_offset, region.source_extent, "source", "Image blit", blit.srcOffsets);
        ResolveBlitBox(destination, region.destination, region.destination_offset, region.destination_extent, "destination", "Image blit",
                       blit.dstOffsets);
        blit_regions[i] = blit;
    }
    vkCmdBlitImage(m_native_command_buffer, source.GetNativeImage(), static_cast<VkImageLayout>(source_layout),
                   destination.GetNativeImage(), static_cast<VkImageLayout>(destination_layout),
                   static_cast<u32>(blit_regions.GetSize()), blit_regions.GetData(), ToVkFilter(filter));
}

static VkAttachmentLoadOp ToVkLoadOp(Rndr::Forge::AttachmentLoadOperation op)
{
    switch (op)
    {
        case Rndr::Forge::AttachmentLoadOperation::Load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case Rndr::Forge::AttachmentLoadOperation::Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case Rndr::Forge::AttachmentLoadOperation::DontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default:
            throw Opal::Exception("Unsupported attachment load operation");
    }
}

static VkAttachmentStoreOp ToVkStoreOp(Rndr::Forge::AttachmentStoreOperation op)
{
    switch (op)
    {
        case Rndr::Forge::AttachmentStoreOperation::Store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case Rndr::Forge::AttachmentStoreOperation::DontCare:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default:
            throw Opal::Exception("Unsupported attachment store operation");
    }
}

void Rndr::Forge::CommandBuffer::CmdBeginRendering(const RenderingDesc& desc)
{
    Opal::DynamicArray<VkRenderingAttachmentInfo> color_attachments;
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

void Rndr::Forge::CommandBuffer::CmdEndRendering()
{
    vkCmdEndRendering(m_native_command_buffer);
}

void Rndr::Forge::CommandBuffer::CmdSetViewport(const Vector2f& offset, const Vector2f& extent, f32 min_depth, f32 max_depth)
{
    const VkViewport viewport{
        .x = offset.x, .y = offset.y, .width = extent.x, .height = extent.y, .minDepth = min_depth, .maxDepth = max_depth};
    vkCmdSetViewport(m_native_command_buffer, 0, 1, &viewport);
}

void Rndr::Forge::CommandBuffer::CmdSetScissor(const Vector2i& offset, const Vector2i& extent)
{
    const VkRect2D scissor{.offset = {.x = offset.x, .y = offset.y},
                           .extent = {.width = static_cast<u32>(extent.x), .height = static_cast<u32>(extent.y)}};
    vkCmdSetScissor(m_native_command_buffer, 0, 1, &scissor);
}

void Rndr::Forge::CommandBuffer::CmdBindVertexBuffer(const Buffer& buffer, u32 binding, u64 offset)
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

void Rndr::Forge::CommandBuffer::CmdBindIndexBuffer(const Buffer& buffer, u64 offset, IndexSize index_size)
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

void Rndr::Forge::CommandBuffer::CmdBindPipeline(const Pipeline& pipeline)
{
    vkCmdBindPipeline(m_native_command_buffer, pipeline.GetBindPoint(), pipeline.GetNativePipeline());
}

void Rndr::Forge::CommandBuffer::CmdBindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& descriptor_set,
                                                       u32 first_set)
{
    const VkDescriptorSet native_set = descriptor_set.GetNativeDescriptorSet();
    vkCmdBindDescriptorSets(m_native_command_buffer, pipeline.GetBindPoint(), pipeline.GetNativePipelineLayout(), first_set, 1, &native_set,
                            0, nullptr);
}

void Rndr::Forge::CommandBuffer::CmdBindDescriptorSets(const Pipeline& pipeline,
                                                        Opal::ArrayView<const Opal::Ref<const DescriptorSet>> descriptor_sets,
                                                        u32 first_set)
{
    Opal::DynamicArray<VkDescriptorSet> native_sets;
    for (const auto& set : descriptor_sets)
    {
        native_sets.PushBack(set->GetNativeDescriptorSet());
    }
    vkCmdBindDescriptorSets(m_native_command_buffer, pipeline.GetBindPoint(), pipeline.GetNativePipelineLayout(), first_set,
                            static_cast<u32>(native_sets.GetSize()), native_sets.GetData(), 0, nullptr);
}

void Rndr::Forge::CommandBuffer::CmdPushConstants(const Pipeline& pipeline, ShaderTypeBits shader_stages,
                                                   Opal::ArrayView<const u8> data, u32 offset)
{
    vkCmdPushConstants(m_native_command_buffer, pipeline.GetNativePipelineLayout(), ToVkShaderStageFlags(shader_stages), offset,
                       static_cast<u32>(data.GetSize()), data.GetData());
}

void Rndr::Forge::CommandBuffer::CmdDrawIndexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset,
                                                 u32 first_instance)
{
    vkCmdDrawIndexed(m_native_command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

/**
 * The checks every indirect command shares: the buffer has to allow indirect use, the offset has to be
 * aligned, and every command the device will read has to fit inside the buffer.
 */
static void ValidateIndirectRange(const Rndr::Forge::Buffer& buffer, Rndr::u64 offset, Rndr::u32 count, Rndr::u32 stride,
                                  Rndr::u64 command_size, const char* what)
{
    using namespace Rndr;
    if (!(buffer.GetDesc().usage & Forge::BufferUsageBits::IndirectBuffer))
    {
        throw Opal::Exception(Opal::StringEx(what) + " needs a buffer created with BufferUsageBits::IndirectBuffer!");
    }
    if (offset % 4 != 0)
    {
        throw Opal::Exception(Opal::StringEx(what) + " offset must be a multiple of 4!");
    }
    if (count == 0)
    {
        return;
    }
    if (count > 1 && (stride % 4 != 0 || stride < command_size))
    {
        throw Opal::Exception(Opal::StringEx(what) + " stride must be a multiple of 4 and at least one command long!");
    }
    // Written so that neither the offset nor the span of the commands can overflow the sum and pass, the way
    // Buffer::Update checks it. The last command starts at count - 1 strides in, so only that many are counted.
    const u64 buffer_size = buffer.GetSize();
    const u64 span_before_last = static_cast<u64>(count - 1) * stride;
    if (offset > buffer_size || span_before_last > buffer_size - offset || command_size > buffer_size - offset - span_before_last)
    {
        throw Opal::Exception(Opal::StringEx(what) + " reaches past the end of the buffer!");
    }
}

void Rndr::Forge::CommandBuffer::CmdDraw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance)
{
    vkCmdDraw(m_native_command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void Rndr::Forge::CommandBuffer::CmdDrawIndirect(const Buffer& buffer, u64 offset, u32 draw_count, u32 stride)
{
    ValidateIndirectRange(buffer, offset, draw_count, stride, sizeof(DrawIndirectCommand), "Indirect draw");
    vkCmdDrawIndirect(m_native_command_buffer, buffer.GetNativeBuffer(), offset, draw_count, stride);
}

void Rndr::Forge::CommandBuffer::CmdDrawIndexedIndirect(const Buffer& buffer, u64 offset, u32 draw_count, u32 stride)
{
    ValidateIndirectRange(buffer, offset, draw_count, stride, sizeof(DrawIndexedIndirectCommand), "Indirect indexed draw");
    vkCmdDrawIndexedIndirect(m_native_command_buffer, buffer.GetNativeBuffer(), offset, draw_count, stride);
}

void Rndr::Forge::CommandBuffer::CmdDrawMeshTasks(u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
    // The loader hands out a callable trampoline whether or not the device enabled the extension, so a null
    // check does not catch this - calling it without the extension is an access violation, not a failure.
    if (!m_device->IsExtensionEnabled(VK_EXT_MESH_SHADER_EXTENSION_NAME) || vkCmdDrawMeshTasksEXT == nullptr)
    {
        throw Opal::Exception("Mesh shader drawing needs VK_EXT_mesh_shader, which the device did not enable!");
    }
    vkCmdDrawMeshTasksEXT(m_native_command_buffer, group_count_x, group_count_y, group_count_z);
}

void Rndr::Forge::CommandBuffer::CmdDispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
    vkCmdDispatch(m_native_command_buffer, group_count_x, group_count_y, group_count_z);
}

void Rndr::Forge::CommandBuffer::CmdDispatchIndirect(const Buffer& buffer, u64 offset)
{
    ValidateIndirectRange(buffer, offset, 1, sizeof(DispatchIndirectCommand), sizeof(DispatchIndirectCommand),
                          "Indirect dispatch");
    vkCmdDispatchIndirect(m_native_command_buffer, buffer.GetNativeBuffer(), offset);
}

Rndr::Forge::CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
    : m_device(std::move(other.m_device)), m_queue(std::move(other.m_queue)), m_native_command_buffer(other.m_native_command_buffer)
{
    other.m_native_command_buffer = VK_NULL_HANDLE;
}

Rndr::Forge::CommandBuffer& Rndr::Forge::CommandBuffer::operator=(CommandBuffer&& other) noexcept
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
