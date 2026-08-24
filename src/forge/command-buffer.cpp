#include "rndr/forge/command-buffer.hpp"

#include "opal/container/in-place-array.h"

#include "rndr/forge/buffer.hpp"
#include "rndr/forge/descriptor-set.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/pipeline.hpp"
#include "rndr/forge/query.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/log.hpp"

Opal::Expected<Rndr::Forge::CommandBuffer, Rndr::ErrorCode> Rndr::Forge::CommandBuffer::Create(const Device& device, DeviceQueue& queue)
{
    using Result = Opal::Expected<CommandBuffer, ErrorCode>;

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = queue.GetNativeCommandPool();
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    CommandBuffer command_buffer;
    command_buffer.m_device = &device;
    command_buffer.m_queue = &queue;
    RNDR_FORGE_VK_CHECK_EXPECTED(vkAllocateCommandBuffers(device.GetNativeDevice(), &alloc_info, &command_buffer.m_native_command_buffer),
                                 "vkAllocateCommandBuffers", Result);
    return Result(std::move(command_buffer));
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

Rndr::ErrorCode Rndr::Forge::CommandBuffer::Begin(bool submit_one_time) const
{
    VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0};
    begin_info.flags |= submit_one_time ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;

    RNDR_FORGE_VK_CHECK(vkBeginCommandBuffer(m_native_command_buffer, &begin_info), "vkBeginCommandBuffer");
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::End() const
{
    RNDR_FORGE_VK_CHECK(vkEndCommandBuffer(m_native_command_buffer), "vkEndCommandBuffer");
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::Reset() const
{
    RNDR_FORGE_VK_CHECK(vkResetCommandBuffer(m_native_command_buffer, 0), "vkResetCommandBuffer");
    return ErrorCode::Success;
}

static VkImageMemoryBarrier2 ToVkImageBarrier(const Rndr::Forge::TextureBarrier& texture_barrier)
{
    const Rndr::Forge::Texture& texture = texture_barrier.texture.Get();
    const Rndr::Forge::ImageSubresourceRange& range = texture_barrier.subresource_range;
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = static_cast<VkPipelineStageFlags2>(texture_barrier.stages_must_finish),
        .srcAccessMask = static_cast<VkAccessFlags2>(texture_barrier.stages_must_finish_access),
        .dstStageMask = static_cast<VkPipelineStageFlags2>(texture_barrier.before_stages_start),
        .dstAccessMask = static_cast<VkAccessFlags2>(texture_barrier.before_stages_start_access),
        .oldLayout = static_cast<VkImageLayout>(texture_barrier.old_layout),
        .newLayout = static_cast<VkImageLayout>(texture_barrier.new_layout),
        // Ignored unless the caller is transferring ownership. Leaving these zero would name family zero,
        // which a barrier on an image created with VK_SHARING_MODE_CONCURRENT is not allowed to do.
        .srcQueueFamilyIndex = texture_barrier.source_queue_family,
        .dstQueueFamilyIndex = texture_barrier.destination_queue_family,
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
        .srcQueueFamilyIndex = buffer_barrier.source_queue_family,
        .dstQueueFamilyIndex = buffer_barrier.destination_queue_family,
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

/**
 * A batch of translated barriers that stays on the stack until it does not fit. A barrier batch happens every
 * frame and is nearly always tiny, and the obvious alternative - Opal's scratch allocator - asserts unless
 * the application pushed one, which nothing here does.
 */
template <typename T, Rndr::i32 k_in_place_count>
class BarrierBatch
{
public:
    explicit BarrierBatch(Rndr::i32 count) : m_count(count)
    {
        if (count > k_in_place_count)
        {
            m_heap.Resize(count);
        }
    }

    T& operator[](Rndr::i32 index) { return m_count > k_in_place_count ? m_heap[index] : m_in_place[index]; }
    [[nodiscard]] const T* GetData() const { return m_count > k_in_place_count ? m_heap.GetData() : m_in_place.GetData(); }
    [[nodiscard]] Rndr::u32 GetCount() const { return static_cast<Rndr::u32>(m_count); }

private:
    Rndr::i32 m_count = 0;
    Opal::InPlaceArray<T, k_in_place_count> m_in_place;
    Opal::DynamicArray<T> m_heap;
};

/** Every stage the barriers of one dependency name, on either side. */
template <typename Barrier>
Rndr::Forge::PipelineStageBits CollectStages(Opal::ArrayView<const Barrier> barriers)
{
    Rndr::Forge::PipelineStageBits stages = Rndr::Forge::PipelineStageBits::None;
    for (const Barrier& barrier : barriers)
    {
        stages |= barrier.stages_must_finish | barrier.before_stages_start;
    }
    return stages;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBarriers(const Barriers& barriers)
{
    if (barriers.memory.IsEmpty() && barriers.buffer.IsEmpty() && barriers.texture.IsEmpty())
    {
        return ErrorCode::Success;
    }
    // The task and mesh stages belong to an extension, and naming one the device did not enable is a
    // validation error rather than something the driver ignores.
    const PipelineStageBits all_stages = CollectStages(barriers.memory) | CollectStages(barriers.buffer) | CollectStages(barriers.texture);
    if (!!(all_stages & (PipelineStageBits::TaskShader | PipelineStageBits::MeshShader)) &&
        !m_device->IsExtensionEnabled(VK_EXT_MESH_SHADER_EXTENSION_NAME))
    {
        RNDR_LOG_ERROR("Forge: a barrier naming the task or mesh stage needs VK_EXT_mesh_shader, which the device did not enable");
        return ErrorCode::InvalidArgument;
    }
    // Eight of each covers every batch this repository issues, and a bigger one still works.
    constexpr i32 k_in_place_count = 8;
    BarrierBatch<VkMemoryBarrier2, k_in_place_count> memory_barriers(static_cast<i32>(barriers.memory.GetSize()));
    for (i32 i = 0; i < barriers.memory.GetSize(); ++i)
    {
        memory_barriers[i] = ToVkMemoryBarrier(barriers.memory[i]);
    }
    BarrierBatch<VkBufferMemoryBarrier2, k_in_place_count> buffer_barriers(static_cast<i32>(barriers.buffer.GetSize()));
    for (i32 i = 0; i < barriers.buffer.GetSize(); ++i)
    {
        buffer_barriers[i] = ToVkBufferBarrier(barriers.buffer[i]);
    }
    BarrierBatch<VkImageMemoryBarrier2, k_in_place_count> image_barriers(static_cast<i32>(barriers.texture.GetSize()));
    for (i32 i = 0; i < barriers.texture.GetSize(); ++i)
    {
        image_barriers[i] = ToVkImageBarrier(barriers.texture[i]);
    }

    const VkDependencyInfo dependency_info{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                           .dependencyFlags = static_cast<VkDependencyFlags>(barriers.flags),
                                           .memoryBarrierCount = memory_barriers.GetCount(),
                                           .pMemoryBarriers = memory_barriers.GetData(),
                                           .bufferMemoryBarrierCount = buffer_barriers.GetCount(),
                                           .pBufferMemoryBarriers = buffer_barriers.GetData(),
                                           .imageMemoryBarrierCount = image_barriers.GetCount(),
                                           .pImageMemoryBarriers = image_barriers.GetData()};
    vkCmdPipelineBarrier2(m_native_command_buffer, &dependency_info);

    // Vulkan keeps no record of what layout a texture ended up in, so this is where Forge's own is kept up to
    // date. Recorded rather than executed, which is what makes it the caller's job to record in order.
    for (const TextureBarrier& barrier : barriers.texture)
    {
        RNDR_FORGE_CHECK(barrier.texture.Get().SetCurrentLayout(barrier.subresource_range, barrier.new_layout));
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdTextureBarrier(const TextureBarrier& texture_barrier)
{
    return CmdBarriers({.texture = {&texture_barrier, 1}});
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdTextureBarrier(const Opal::Expected<TextureBarrier, ErrorCode>& texture_barrier)
{
    if (!texture_barrier.HasValue())
    {
        return texture_barrier.GetError();
    }
    return CmdTextureBarrier(texture_barrier.GetValue());
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdTextureBarriers(Opal::ArrayView<const TextureBarrier> texture_barriers)
{
    return CmdBarriers({.texture = texture_barriers});
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBufferBarrier(const BufferBarrier& buffer_barrier)
{
    return CmdBarriers({.buffer = {&buffer_barrier, 1}});
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBufferBarriers(Opal::ArrayView<const BufferBarrier> buffer_barriers)
{
    return CmdBarriers({.buffer = buffer_barriers});
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdMemoryBarrier(const MemoryBarrier& memory_barrier)
{
    return CmdBarriers({.memory = {&memory_barrier, 1}});
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdTransition(Texture& texture, ImageLayout new_layout)
{
    const Opal::Expected<ImageLayout, ErrorCode> old_layout = texture.GetCurrentLayout();
    if (!old_layout.HasValue())
    {
        return old_layout.GetError();
    }
    return CmdTextureBarrier(TextureBarrier::To(texture, old_layout.GetValue(), new_layout));
}

/** The extent of one mip level of a texture, which is the base extent halved once per level, floored at one. */
static Rndr::u32 MipExtent(Rndr::u32 base_extent, Rndr::u32 mip_level)
{
    return Opal::Max(1u, base_extent >> mip_level);
}

/** The buffer has to allow the transfer it is about to take part in. */
static Rndr::ErrorCode ValidateBufferUsage(const Rndr::Forge::Buffer& buffer, Rndr::Forge::BufferUsageBits required, const char* role,
                                           const char* what)
{
    if (!(buffer.GetDesc().usage & required))
    {
        RNDR_LOG_ERROR("Forge: {} needs a {} buffer created with the matching BufferUsageBits", what, role);
        return Rndr::ErrorCode::InvalidArgument;
    }
    return Rndr::ErrorCode::Success;
}

/** The texture has to allow the transfer it is about to take part in. */
static Rndr::ErrorCode ValidateTextureUsage(const Rndr::Forge::Texture& texture, Rndr::Forge::TextureUsageBits required, const char* role,
                                            const char* what)
{
    if (!(texture.GetDesc().usage & required))
    {
        RNDR_LOG_ERROR("Forge: {} needs a {} texture created with the matching TextureUsageBits", what, role);
        return Rndr::ErrorCode::InvalidArgument;
    }
    return Rndr::ErrorCode::Success;
}

/** The range has to fit, written so that neither a large offset nor a large size can overflow the sum and pass. */
static Rndr::ErrorCode ValidateBufferRange(const Rndr::Forge::Buffer& buffer, Rndr::u64 offset, Rndr::u64 size, const char* role,
                                           const char* what)
{
    const Rndr::u64 buffer_size = buffer.GetSize();
    if (offset > buffer_size || size > buffer_size - offset)
    {
        RNDR_LOG_ERROR("Forge: {} reaches past the end of the {} buffer", what, role);
        return Rndr::ErrorCode::OutOfBounds;
    }
    return Rndr::ErrorCode::Success;
}

/**
 * The layout one side of a copy or blit is in, taken from the texture across every region that names it.
 *
 * One Vulkan call carries a single layout per side, so regions whose levels are in different layouts have no
 * answer and are refused. So is a layout the role does not allow: that is the check the API could not make
 * while the caller supplied the layout, and it turns silent undefined behaviour into a message.
 */
template <typename Region>
static Opal::Expected<Rndr::Forge::ImageLayout, Rndr::ErrorCode> ResolveTransferLayout(const Rndr::Forge::Texture& texture,
                                                                                       Opal::ArrayView<const Region> regions,
                                                                                       Rndr::Forge::ImageSubresourceLayers Region::* member,
                                                                                       bool as_source, const char* role, const char* what)
{
    using namespace Rndr;
    using Result = Opal::Expected<Forge::ImageLayout, ErrorCode>;

    Forge::ImageLayout layout = Forge::ImageLayout::Undefined;
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        const Forge::ImageSubresourceLayers& subresource = regions[i].*member;
        const Forge::ImageSubresourceRange range{.aspect_mask = subresource.aspect_mask,
                                                 .first_mip_level = subresource.mip_level,
                                                 .mip_level_count = 1,
                                                 .first_array_layer = subresource.first_array_layer,
                                                 .array_layer_count = subresource.array_layer_count};
        const Opal::Expected<Forge::ImageLayout, ErrorCode> region_layout = texture.GetCurrentLayout(range);
        if (!region_layout.HasValue())
        {
            return Result(region_layout.GetError());
        }
        if (i != 0 && region_layout.GetValue() != layout)
        {
            RNDR_LOG_ERROR("Forge: {} names levels of the {} texture that are in different layouts, {} and {}", what, role,
                           Forge::ImageLayoutToString(layout), Forge::ImageLayoutToString(region_layout.GetValue()));
            return Result(ErrorCode::InvalidArgument);
        }
        layout = region_layout.GetValue();
    }
    const Forge::ImageLayout required = as_source ? Forge::ImageLayout::TransferSource : Forge::ImageLayout::TransferDestination;
    if (layout != required && layout != Forge::ImageLayout::General)
    {
        RNDR_LOG_ERROR("Forge: {} needs the {} texture in the {} or General layout, and it is in {}", what, role,
                       Forge::ImageLayoutToString(required), Forge::ImageLayoutToString(layout));
        return Result(ErrorCode::InvalidArgument);
    }
    return Result(layout);
}

/**
 * Check that the subresource exists on the texture and report the extent of the mip level it names, which is
 * what every region on that texture is measured against.
 */
static Opal::Expected<VkExtent3D, Rndr::ErrorCode> ValidateSubresource(const Rndr::Forge::Texture& texture,
                                                                       const Rndr::Forge::ImageSubresourceLayers& subresource,
                                                                       const char* role, const char* what)
{
    using namespace Rndr;
    using Result = Opal::Expected<VkExtent3D, ErrorCode>;

    const Forge::TextureDesc& desc = texture.GetDesc();
    if (subresource.mip_level >= desc.mip_level_count)
    {
        RNDR_LOG_ERROR("Forge: {} names a mip level the {} texture does not have", what, role);
        return Result(ErrorCode::OutOfBounds);
    }
    if (subresource.array_layer_count == 0 || subresource.first_array_layer > desc.array_layer_count ||
        subresource.array_layer_count > desc.array_layer_count - subresource.first_array_layer)
    {
        RNDR_LOG_ERROR("Forge: {} names array layers the {} texture does not have", what, role);
        return Result(ErrorCode::OutOfBounds);
    }
    return Result(VkExtent3D{.width = MipExtent(desc.width, subresource.mip_level),
                             .height = MipExtent(desc.height, subresource.mip_level),
                             .depth = MipExtent(desc.depth, subresource.mip_level)});
}

/**
 * Resolve the texture side of a copy: check that the subresource exists on the texture, fill a zero extent in
 * with the rest of the mip level, and check that the box fits inside it.
 */
static Opal::Expected<VkExtent3D, Rndr::ErrorCode> ResolveTextureRegion(const Rndr::Forge::Texture& texture,
                                                                        const Rndr::Forge::ImageSubresourceLayers& subresource,
                                                                        const Rndr::Vector3i& offset, const Rndr::Vector3i& extent,
                                                                        const char* role, const char* what)
{
    using namespace Rndr;
    using Result = Opal::Expected<VkExtent3D, ErrorCode>;

    const Opal::Expected<VkExtent3D, ErrorCode> mip_extent_result = ValidateSubresource(texture, subresource, role, what);
    if (!mip_extent_result.HasValue())
    {
        return Result(mip_extent_result.GetError());
    }
    const VkExtent3D mip_extent = mip_extent_result.GetValue();
    if (offset.x < 0 || offset.y < 0 || offset.z < 0 || extent.x < 0 || extent.y < 0 || extent.z < 0)
    {
        RNDR_LOG_ERROR("Forge: {} has a negative offset or extent on the {} texture", what, role);
        return Result(ErrorCode::InvalidArgument);
    }
    const u32 offset_x = static_cast<u32>(offset.x);
    const u32 offset_y = static_cast<u32>(offset.y);
    const u32 offset_z = static_cast<u32>(offset.z);
    if (offset_x > mip_extent.width || offset_y > mip_extent.height || offset_z > mip_extent.depth)
    {
        RNDR_LOG_ERROR("Forge: {} starts outside the mip level of the {} texture", what, role);
        return Result(ErrorCode::OutOfBounds);
    }
    // A zero extent on an axis means the rest of the mip level past the offset on that axis.
    const VkExtent3D resolved{.width = extent.x != 0 ? static_cast<u32>(extent.x) : mip_extent.width - offset_x,
                              .height = extent.y != 0 ? static_cast<u32>(extent.y) : mip_extent.height - offset_y,
                              .depth = extent.z != 0 ? static_cast<u32>(extent.z) : mip_extent.depth - offset_z};
    if (resolved.width > mip_extent.width - offset_x || resolved.height > mip_extent.height - offset_y ||
        resolved.depth > mip_extent.depth - offset_z)
    {
        RNDR_LOG_ERROR("Forge: {} reaches past the mip level of the {} texture", what, role);
        return Result(ErrorCode::OutOfBounds);
    }
    return Result(resolved);
}

/**
 * Resolve one side of a blit into the two corners Vulkan wants. A blit names corners rather than an extent so
 * that it can run an axis backwards, which is how it mirrors, so a negative extent is allowed here and the
 * far corner can sit before the near one.
 */
static Rndr::ErrorCode ResolveBlitBox(const Rndr::Forge::Texture& texture, const Rndr::Forge::ImageSubresourceLayers& subresource,
                                      const Rndr::Vector3i& offset, const Rndr::Vector3i& extent, const char* role, const char* what,
                                      VkOffset3D corners[2])
{
    using namespace Rndr;
    const Opal::Expected<VkExtent3D, ErrorCode> mip_extent_result = ValidateSubresource(texture, subresource, role, what);
    if (!mip_extent_result.HasValue())
    {
        return mip_extent_result.GetError();
    }
    const VkExtent3D mip_extent = mip_extent_result.GetValue();
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
            RNDR_LOG_ERROR("Forge: {} reaches past the mip level of the {} texture", what, role);
            return ErrorCode::OutOfBounds;
        }
        if (near_corner[axis] == far_corner[axis])
        {
            RNDR_LOG_ERROR("Forge: {} has an empty box on the {} texture", what, role);
            return ErrorCode::InvalidArgument;
        }
    }
    corners[0] = {.x = near_corner[0], .y = near_corner[1], .z = near_corner[2]};
    corners[1] = {.x = far_corner[0], .y = far_corner[1], .z = far_corner[2]};
    return ErrorCode::Success;
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
static Opal::Expected<VkBufferImageCopy, Rndr::ErrorCode> ToVkBufferImageCopy(const Rndr::Forge::Buffer& buffer,
                                                                              const Rndr::Forge::Texture& texture,
                                                                              const Rndr::Forge::BufferTextureCopyRegion& region,
                                                                              const char* buffer_role, const char* what)
{
    using namespace Rndr;
    using Result = Opal::Expected<VkBufferImageCopy, ErrorCode>;

    const Opal::Expected<VkExtent3D, ErrorCode> extent_result =
        ResolveTextureRegion(texture, region.texture_subresource, region.texture_offset, region.texture_extent, "target", what);
    if (!extent_result.HasValue())
    {
        return Result(extent_result.GetError());
    }
    const VkExtent3D extent = extent_result.GetValue();
    if (region.buffer_offset % 4 != 0)
    {
        RNDR_LOG_ERROR("Forge: the {} buffer offset must be a multiple of 4, and is {}", what, region.buffer_offset);
        return Result(ErrorCode::InvalidArgument);
    }
    // A compressed format is measured in blocks rather than pixels, so the size below would be wrong for one.
    // GetPixelSize reports zero for those, and the validation layer covers what this cannot.
    const u32 pixel_size = GetPixelSize(texture.GetDesc().format);
    if (pixel_size != 0)
    {
        const u64 row_length = region.buffer_row_length != 0 ? region.buffer_row_length : extent.width;
        const u64 image_height = region.buffer_layer_height != 0 ? region.buffer_layer_height : extent.height;
        const u64 region_size =
            static_cast<u64>(pixel_size) * row_length * image_height * extent.depth * region.texture_subresource.array_layer_count;
        RNDR_FORGE_CHECK_EXPECTED(ValidateBufferRange(buffer, region.buffer_offset, region_size, buffer_role, what), Result);
    }
    return Result(
        VkBufferImageCopy{.bufferOffset = region.buffer_offset,
                          .bufferRowLength = region.buffer_row_length,
                          .bufferImageHeight = region.buffer_layer_height,
                          .imageSubresource = ToVkSubresourceLayers(region.texture_subresource, texture.GetDesc().format),
                          .imageOffset = {.x = region.texture_offset.x, .y = region.texture_offset.y, .z = region.texture_offset.z},
                          .imageExtent = extent});
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdCopyBuffer(const Buffer& source, const Buffer& destination,
                                                          Opal::ArrayView<const BufferCopyRegion> regions)
{
    if (regions.IsEmpty())
    {
        return ErrorCode::Success;
    }
    RNDR_FORGE_CHECK(ValidateBufferUsage(source, BufferUsageBits::TransferSource, "source", "Buffer copy"));
    RNDR_FORGE_CHECK(ValidateBufferUsage(destination, BufferUsageBits::TransferDestination, "destination", "Buffer copy"));
    Opal::DynamicArray<VkBufferCopy> copy_regions(regions.GetSize());
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        const BufferCopyRegion& region = regions[i];
        if (region.source_offset > source.GetSize() || region.destination_offset > destination.GetSize())
        {
            RNDR_LOG_ERROR("Forge: a buffer copy starts past the end of a buffer");
            return ErrorCode::OutOfBounds;
        }
        // The whole buffer means as much as both sides have left, so that neither end is overrun.
        const u64 size = region.size == k_whole_buffer
                             ? Opal::Min(source.GetSize() - region.source_offset, destination.GetSize() - region.destination_offset)
                             : region.size;
        RNDR_FORGE_CHECK(ValidateBufferRange(source, region.source_offset, size, "source", "Buffer copy"));
        RNDR_FORGE_CHECK(ValidateBufferRange(destination, region.destination_offset, size, "destination", "Buffer copy"));
        copy_regions[i] = {.srcOffset = region.source_offset, .dstOffset = region.destination_offset, .size = size};
    }
    vkCmdCopyBuffer(m_native_command_buffer, source.GetNativeBuffer(), destination.GetNativeBuffer(),
                    static_cast<u32>(copy_regions.GetSize()), copy_regions.GetData());
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdCopyBuffer(const Buffer& source, const Buffer& destination)
{
    const BufferCopyRegion region;
    return CmdCopyBuffer(source, destination, {&region, 1});
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdCopyBufferToTexture(const Buffer& buffer, Texture& texture,
                                                                   Opal::ArrayView<const BufferTextureCopyRegion> regions)
{
    if (regions.IsEmpty())
    {
        return ErrorCode::Success;
    }
    RNDR_FORGE_CHECK(ValidateBufferUsage(buffer, BufferUsageBits::TransferSource, "source", "Buffer to texture copy"));
    RNDR_FORGE_CHECK(ValidateTextureUsage(texture, TextureUsageBits::TransferDestination, "destination", "Buffer to texture copy"));
    Opal::DynamicArray<VkBufferImageCopy> copy_regions(regions.GetSize());
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        Opal::Expected<VkBufferImageCopy, ErrorCode> copy_region =
            ToVkBufferImageCopy(buffer, texture, regions[i], "source", "Buffer to texture copy");
        if (!copy_region.HasValue())
        {
            return copy_region.GetError();
        }
        copy_regions[i] = copy_region.GetValue();
    }
    const Opal::Expected<ImageLayout, ErrorCode> texture_layout = ResolveTransferLayout(
        texture, regions, &BufferTextureCopyRegion::texture_subresource, false, "destination", "Buffer to texture copy");
    if (!texture_layout.HasValue())
    {
        return texture_layout.GetError();
    }
    vkCmdCopyBufferToImage(m_native_command_buffer, buffer.GetNativeBuffer(), texture.GetNativeImage(),
                           static_cast<VkImageLayout>(texture_layout.GetValue()), static_cast<u32>(copy_regions.GetSize()),
                           copy_regions.GetData());
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdCopyBufferToTexture(const Buffer& buffer, const Bitmap& bitmap, Texture& texture)
{
    // One region per mip level, laid out the way the bitmap packs them. The aspect and the extent come from the
    // texture through the general path below, so this no longer assumes a color format.
    Opal::DynamicArray<BufferTextureCopyRegion> regions(bitmap.GetMipCount());
    for (u32 mip_level = 0; mip_level < bitmap.GetMipCount(); ++mip_level)
    {
        regions[mip_level] = {.buffer_offset = bitmap.GetMipLevelOffset(static_cast<i32>(mip_level)),
                              .texture_subresource = {.mip_level = mip_level}};
    }
    return CmdCopyBufferToTexture(buffer, texture, regions);
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdCopyTextureToBuffer(const Texture& texture, const Buffer& buffer,
                                                                   Opal::ArrayView<const BufferTextureCopyRegion> regions)
{
    if (regions.IsEmpty())
    {
        return ErrorCode::Success;
    }
    RNDR_FORGE_CHECK(ValidateTextureUsage(texture, TextureUsageBits::TransferSource, "source", "Texture to buffer copy"));
    RNDR_FORGE_CHECK(ValidateBufferUsage(buffer, BufferUsageBits::TransferDestination, "destination", "Texture to buffer copy"));
    Opal::DynamicArray<VkBufferImageCopy> copy_regions(regions.GetSize());
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        Opal::Expected<VkBufferImageCopy, ErrorCode> copy_region =
            ToVkBufferImageCopy(buffer, texture, regions[i], "destination", "Texture to buffer copy");
        if (!copy_region.HasValue())
        {
            return copy_region.GetError();
        }
        copy_regions[i] = copy_region.GetValue();
    }
    const Opal::Expected<ImageLayout, ErrorCode> texture_layout =
        ResolveTransferLayout(texture, regions, &BufferTextureCopyRegion::texture_subresource, true, "source", "Texture to buffer copy");
    if (!texture_layout.HasValue())
    {
        return texture_layout.GetError();
    }
    vkCmdCopyImageToBuffer(m_native_command_buffer, texture.GetNativeImage(), static_cast<VkImageLayout>(texture_layout.GetValue()),
                           buffer.GetNativeBuffer(), static_cast<u32>(copy_regions.GetSize()), copy_regions.GetData());
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdCopyTexture(const Texture& source, Texture& destination,
                                                           Opal::ArrayView<const TextureCopyRegion> regions)
{
    if (regions.IsEmpty())
    {
        return ErrorCode::Success;
    }
    RNDR_FORGE_CHECK(ValidateTextureUsage(source, TextureUsageBits::TransferSource, "source", "Texture copy"));
    RNDR_FORGE_CHECK(ValidateTextureUsage(destination, TextureUsageBits::TransferDestination, "destination", "Texture copy"));
    Opal::DynamicArray<VkImageCopy> copy_regions(regions.GetSize());
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        const TextureCopyRegion& region = regions[i];
        // The extent is resolved against the source and then checked against the destination, so a zero extent
        // means the rest of the source mip level and still has to fit where it is going.
        const Opal::Expected<VkExtent3D, ErrorCode> extent_result =
            ResolveTextureRegion(source, region.source, region.source_offset, region.extent, "source", "Texture copy");
        if (!extent_result.HasValue())
        {
            return extent_result.GetError();
        }
        const VkExtent3D extent = extent_result.GetValue();
        const Vector3i resolved_extent{static_cast<i32>(extent.width), static_cast<i32>(extent.height), static_cast<i32>(extent.depth)};
        const Opal::Expected<VkExtent3D, ErrorCode> destination_extent = ResolveTextureRegion(
            destination, region.destination, region.destination_offset, resolved_extent, "destination", "Texture copy");
        if (!destination_extent.HasValue())
        {
            return destination_extent.GetError();
        }
        copy_regions[i] = {
            .srcSubresource = ToVkSubresourceLayers(region.source, source.GetDesc().format),
            .srcOffset = {.x = region.source_offset.x, .y = region.source_offset.y, .z = region.source_offset.z},
            .dstSubresource = ToVkSubresourceLayers(region.destination, destination.GetDesc().format),
            .dstOffset = {.x = region.destination_offset.x, .y = region.destination_offset.y, .z = region.destination_offset.z},
            .extent = extent};
    }
    const Opal::Expected<ImageLayout, ErrorCode> source_layout =
        ResolveTransferLayout(source, regions, &TextureCopyRegion::source, true, "source", "Texture copy");
    if (!source_layout.HasValue())
    {
        return source_layout.GetError();
    }
    const Opal::Expected<ImageLayout, ErrorCode> destination_layout =
        ResolveTransferLayout(destination, regions, &TextureCopyRegion::destination, false, "destination", "Texture copy");
    if (!destination_layout.HasValue())
    {
        return destination_layout.GetError();
    }
    vkCmdCopyImage(m_native_command_buffer, source.GetNativeImage(), static_cast<VkImageLayout>(source_layout.GetValue()),
                   destination.GetNativeImage(), static_cast<VkImageLayout>(destination_layout.GetValue()),
                   static_cast<u32>(copy_regions.GetSize()), copy_regions.GetData());
    return ErrorCode::Success;
}

static Opal::Optional<VkFilter> ToVkFilter(Rndr::ImageFilter filter)
{
    switch (filter)
    {
        case Rndr::ImageFilter::Nearest:
            return Opal::Optional<VkFilter>(VK_FILTER_NEAREST);
        case Rndr::ImageFilter::Linear:
            return Opal::Optional<VkFilter>(VK_FILTER_LINEAR);
        default:
            return {};
    }
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBlitTexture(const Texture& source, Texture& destination,
                                                           Opal::ArrayView<const TextureBlitRegion> regions, ImageFilter filter)
{
    if (regions.IsEmpty())
    {
        return ErrorCode::Success;
    }
    RNDR_FORGE_CHECK(ValidateTextureUsage(source, TextureUsageBits::TransferSource, "source", "Texture blit"));
    RNDR_FORGE_CHECK(ValidateTextureUsage(destination, TextureUsageBits::TransferDestination, "destination", "Texture blit"));
    // Blitting is per format and per side, and a format that cannot be blitted is a driver-specific surprise
    // rather than a mistake in the calling code, so it is worth naming here instead of at the validation layer.
    const PhysicalDevice& physical_device = m_device->GetPhysicalDevice();
    if (!physical_device.SupportsBlit(source.GetDesc().format, true))
    {
        RNDR_LOG_ERROR("Forge: this device cannot blit from the format of the source texture");
        return ErrorCode::FeatureNotSupported;
    }
    if (!physical_device.SupportsBlit(destination.GetDesc().format, false))
    {
        RNDR_LOG_ERROR("Forge: this device cannot blit into the format of the destination texture");
        return ErrorCode::FeatureNotSupported;
    }
    if (filter == ImageFilter::Linear && !physical_device.SupportsLinearFilter(source.GetDesc().format))
    {
        RNDR_LOG_ERROR("Forge: this device cannot filter the format of the source texture linearly");
        return ErrorCode::FeatureNotSupported;
    }
    RNDR_FORGE_TRANSLATE(vk_filter, ToVkFilter(filter), "the blit filter");
    Opal::DynamicArray<VkImageBlit> blit_regions(regions.GetSize());
    for (i32 i = 0; i < regions.GetSize(); ++i)
    {
        const TextureBlitRegion& region = regions[i];
        VkImageBlit blit{.srcSubresource = ToVkSubresourceLayers(region.source, source.GetDesc().format),
                         .dstSubresource = ToVkSubresourceLayers(region.destination, destination.GetDesc().format)};
        RNDR_FORGE_CHECK(
            ResolveBlitBox(source, region.source, region.source_offset, region.source_extent, "source", "Texture blit", blit.srcOffsets));
        RNDR_FORGE_CHECK(ResolveBlitBox(destination, region.destination, region.destination_offset, region.destination_extent,
                                        "destination", "Texture blit", blit.dstOffsets));
        blit_regions[i] = blit;
    }
    const Opal::Expected<ImageLayout, ErrorCode> source_layout =
        ResolveTransferLayout(source, regions, &TextureBlitRegion::source, true, "source", "Texture blit");
    if (!source_layout.HasValue())
    {
        return source_layout.GetError();
    }
    const Opal::Expected<ImageLayout, ErrorCode> destination_layout =
        ResolveTransferLayout(destination, regions, &TextureBlitRegion::destination, false, "destination", "Texture blit");
    if (!destination_layout.HasValue())
    {
        return destination_layout.GetError();
    }
    vkCmdBlitImage(m_native_command_buffer, source.GetNativeImage(), static_cast<VkImageLayout>(source_layout.GetValue()),
                   destination.GetNativeImage(), static_cast<VkImageLayout>(destination_layout.GetValue()),
                   static_cast<u32>(blit_regions.GetSize()), blit_regions.GetData(), vk_filter);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdGenerateMips(Texture& texture, ImageLayout final_layout)
{
    const TextureDesc& desc = texture.GetDesc();
    if (desc.mip_level_count < 2)
    {
        RNDR_LOG_ERROR("Forge: generating mips needs a texture with more than one mip level");
        return ErrorCode::InvalidArgument;
    }
    // Every level is read from and written to, so both usages are needed whatever the texture is otherwise for.
    RNDR_FORGE_CHECK(ValidateTextureUsage(texture, TextureUsageBits::TransferSource, "source", "Mip generation"));
    RNDR_FORGE_CHECK(ValidateTextureUsage(texture, TextureUsageBits::TransferDestination, "destination", "Mip generation"));

    // The whole texture starts as the destination of the first blit, including the level that already holds the
    // data - it becomes a source one level at a time inside the loop.
    const Opal::Expected<ImageLayout, ErrorCode> current_layout = texture.GetCurrentLayout();
    if (!current_layout.HasValue())
    {
        return current_layout.GetError();
    }
    if (current_layout.GetValue() != ImageLayout::TransferDestination)
    {
        RNDR_FORGE_CHECK(CmdTextureBarrier(TextureBarrier::ToTransferDestination(texture)));
    }
    for (u32 level = 1; level < desc.mip_level_count; ++level)
    {
        TextureBarrier to_source = TextureBarrier::ToTransferSource(texture, ImageLayout::TransferDestination);
        to_source.subresource_range.first_mip_level = level - 1;
        to_source.subresource_range.mip_level_count = 1;
        RNDR_FORGE_CHECK(CmdTextureBarrier(to_source));

        // Both extents are left at zero, which means the whole of each level, so the blit halves as it goes.
        // Every array layer in the one region: the count defaults to one, which would fill the mip chain of
        // layer zero and leave the rest of an array texture holding whatever it was created with.
        const TextureBlitRegion region{.source = {.mip_level = level - 1, .array_layer_count = desc.array_layer_count},
                                       .destination = {.mip_level = level, .array_layer_count = desc.array_layer_count}};
        RNDR_FORGE_CHECK(CmdBlitTexture(texture, texture, {&region, 1}));
    }
    // Every level but the last is a transfer source by now, and the last one is still a transfer destination.
    Opal::Expected<TextureBarrier, ErrorCode> sources_to_final = TextureBarrier::To(texture, ImageLayout::TransferSource, final_layout);
    if (!sources_to_final.HasValue())
    {
        return sources_to_final.GetError();
    }
    sources_to_final.GetValue().subresource_range.first_mip_level = 0;
    sources_to_final.GetValue().subresource_range.mip_level_count = desc.mip_level_count - 1;
    Opal::Expected<TextureBarrier, ErrorCode> last_to_final = TextureBarrier::To(texture, ImageLayout::TransferDestination, final_layout);
    if (!last_to_final.HasValue())
    {
        return last_to_final.GetError();
    }
    last_to_final.GetValue().subresource_range.first_mip_level = desc.mip_level_count - 1;
    last_to_final.GetValue().subresource_range.mip_level_count = 1;
    const TextureBarrier final_barriers[2] = {sources_to_final.GetValue().Clone(), last_to_final.GetValue().Clone()};
    return CmdTextureBarriers({final_barriers, 2});
}

static Opal::Optional<VkAttachmentLoadOp> ToVkLoadOp(Rndr::Forge::AttachmentLoadOperation op)
{
    switch (op)
    {
        case Rndr::Forge::AttachmentLoadOperation::Load:
            return Opal::Optional<VkAttachmentLoadOp>(VK_ATTACHMENT_LOAD_OP_LOAD);
        case Rndr::Forge::AttachmentLoadOperation::Clear:
            return Opal::Optional<VkAttachmentLoadOp>(VK_ATTACHMENT_LOAD_OP_CLEAR);
        case Rndr::Forge::AttachmentLoadOperation::DontCare:
            return Opal::Optional<VkAttachmentLoadOp>(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
        default:
            return {};
    }
}

static Opal::Optional<VkAttachmentStoreOp> ToVkStoreOp(Rndr::Forge::AttachmentStoreOperation op)
{
    switch (op)
    {
        case Rndr::Forge::AttachmentStoreOperation::Store:
            return Opal::Optional<VkAttachmentStoreOp>(VK_ATTACHMENT_STORE_OP_STORE);
        case Rndr::Forge::AttachmentStoreOperation::DontCare:
            return Opal::Optional<VkAttachmentStoreOp>(VK_ATTACHMENT_STORE_OP_DONT_CARE);
        default:
            return {};
    }
}

/**
 * What one rendering attachment contributes to Vulkan: the view of the texture it names, and the layout that
 * texture is in.
 *
 * The layout is read rather than asked for, the way the copies and the blits read theirs. That is what turns
 * the one thing the validation layer cannot catch - an attachment layout that is legal but not the one the
 * barriers before it actually left the texture in - into a message naming the texture and the two layouts.
 *
 * @param role What this attachment is, for the messages: "colour", "depth" or "stencil".
 * @param is_color Whether the role wants the colour attachment layouts or the depth stencil ones.
 */
static Opal::Expected<VkRenderingAttachmentInfo, Rndr::ErrorCode> ToVkRenderingAttachment(
    const Rndr::Forge::RenderingAttachmentDesc& attachment, const char* role, bool is_color)
{
    using namespace Rndr;
    using Result = Opal::Expected<VkRenderingAttachmentInfo, ErrorCode>;

    if (!attachment.texture.IsValid())
    {
        // Most often an attachment that was filled in and never finished. A pass that wants no depth or no
        // stencil leaves the whole attachment absent instead, which is what the default already is.
        RNDR_LOG_ERROR(
            "Forge: a {} attachment that names no texture. Leave the attachment absent instead - for a swap chain, "
            "SwapChain::HasDepth says whether there is one to name.",
            role);
        return Result(ErrorCode::InvalidArgument);
    }
    const Forge::Texture& texture = attachment.texture.Get();
    if (texture.GetNativeImageView() == VK_NULL_HANDLE)
    {
        // A texture whose usage is transfer only, which Vulkan allows no view on and so cannot be rendered into.
        RNDR_LOG_ERROR("Forge: the {} attachment names a texture that has no image view", role);
        return Result(ErrorCode::InvalidArgument);
    }

    // Over the range the view covers rather than over the whole texture, since that is what is rendered into.
    const Opal::Expected<Forge::ImageLayout, ErrorCode> layout_result = texture.GetCurrentLayout(texture.GetDesc().subresource_range);
    if (!layout_result.HasValue())
    {
        return Result(layout_result.GetError());
    }
    const Forge::ImageLayout layout = layout_result.GetValue();
    const bool layout_allowed =
        layout == Forge::ImageLayout::General ||
        (is_color ? layout == Forge::ImageLayout::ColorAttachment
                  : layout == Forge::ImageLayout::DepthStencilAttachment || layout == Forge::ImageLayout::DepthStencilReadOnly);
    if (!layout_allowed)
    {
        const char* allowed = is_color ? "ColorAttachment or General" : "DepthStencilAttachment, DepthStencilReadOnly or General";
        RNDR_LOG_ERROR(
            "Forge: rendering needs the {} attachment texture in the {} layout, and it is in {}. Transition it before the "
            "pass - CmdTransition, or the matching TextureBarrier preset.",
            role, allowed, Forge::ImageLayoutToString(layout));
        return Result(ErrorCode::InvalidArgument);
    }

    RNDR_FORGE_TRANSLATE_EXPECTED(load_op, ToVkLoadOp(attachment.load_operation), "RenderingAttachmentDesc::load_operation", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(store_op, ToVkStoreOp(attachment.store_operation), "RenderingAttachmentDesc::store_operation", Result);
    VkRenderingAttachmentInfo info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = texture.GetNativeImageView(),
        .imageLayout = static_cast<VkImageLayout>(layout),
        .loadOp = load_op,
        .storeOp = store_op,
    };
    // Only a Clear reads the value, so an attachment that loads or discards is left alone whichever kind it
    // carries. Where it is read, the variant says which kind was written - the one thing VkClearValue cannot,
    // being the same union, and so the one misuse the layer cannot catch either.
    if (attachment.load_operation == Forge::AttachmentLoadOperation::Clear)
    {
        if (is_color)
        {
            if (!attachment.clear_value.IsActive<Vector4f>())
            {
                RNDR_LOG_ERROR("Forge: the {} attachment clears to a DepthStencilClearValue. A colour attachment clears to a Vector4f.",
                               role);
                return Result(ErrorCode::InvalidArgument);
            }
            const Vector4f& color = attachment.clear_value.Get<Vector4f>();
            info.clearValue = {.color = {.float32 = {color.r, color.g, color.b, color.a}}};
        }
        else
        {
            if (!attachment.clear_value.IsActive<Forge::DepthStencilClearValue>())
            {
                RNDR_LOG_ERROR(
                    "Forge: the {} attachment clears to a Vector4f. A depth or a stencil attachment clears to a "
                    "DepthStencilClearValue.",
                    role);
                return Result(ErrorCode::InvalidArgument);
            }
            const Forge::DepthStencilClearValue& depth_stencil = attachment.clear_value.Get<Forge::DepthStencilClearValue>();
            info.clearValue = {.depthStencil = {.depth = depth_stencil.depth, .stencil = depth_stencil.stencil}};
        }
    }
    return Result(info);
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBeginRendering(const RenderingDesc& desc)
{
    Opal::DynamicArray<VkRenderingAttachmentInfo> color_attachments;
    for (const auto& attachment : desc.color_attachments)
    {
        Opal::Expected<VkRenderingAttachmentInfo, ErrorCode> info = ToVkRenderingAttachment(attachment, "colour", true);
        if (!info.HasValue())
        {
            return info.GetError();
        }
        color_attachments.PushBack(info.GetValue());
    }

    // Filled only when the desc carries one, and pointed at only then: an absent depth attachment is a
    // pass that renders without depth, not one whose attachment happens to name no texture.
    const bool has_depth = desc.depth_attachment.HasValue();
    VkRenderingAttachmentInfo depth_attachment{};
    if (has_depth)
    {
        Opal::Expected<VkRenderingAttachmentInfo, ErrorCode> info =
            ToVkRenderingAttachment(desc.depth_attachment.GetValue(), "depth", false);
        if (!info.HasValue())
        {
            return info.GetError();
        }
        depth_attachment = info.GetValue();
    }

    // The same shape as the depth attachment, and separate from it on purpose: Vulkan takes the two sides
    // apart even when one texture carries both, so a combined format names the same texture twice.
    const bool has_stencil = desc.stencil_attachment.HasValue();
    VkRenderingAttachmentInfo stencil_attachment{};
    if (has_stencil)
    {
        Opal::Expected<VkRenderingAttachmentInfo, ErrorCode> info =
            ToVkRenderingAttachment(desc.stencil_attachment.GetValue(), "stencil", false);
        if (!info.HasValue())
        {
            return info.GetError();
        }
        stencil_attachment = info.GetValue();
    }

    const VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {.extent = {.width = static_cast<u32>(desc.render_area_extent.x),
                                  .height = static_cast<u32>(desc.render_area_extent.y)}},
        .layerCount = 1,
        .colorAttachmentCount = static_cast<u32>(color_attachments.GetSize()),
        .pColorAttachments = color_attachments.GetData(),
        .pDepthAttachment = has_depth ? &depth_attachment : nullptr,
        .pStencilAttachment = has_stencil ? &stencil_attachment : nullptr,
    };
    vkCmdBeginRendering(m_native_command_buffer, &rendering_info);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdEndRendering()
{
    vkCmdEndRendering(m_native_command_buffer);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdSetViewport(const Vector2f& offset, const Vector2f& extent, f32 min_depth, f32 max_depth)
{
    const VkViewport viewport{
        .x = offset.x, .y = offset.y, .width = extent.x, .height = extent.y, .minDepth = min_depth, .maxDepth = max_depth};
    vkCmdSetViewport(m_native_command_buffer, 0, 1, &viewport);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdSetScissor(const Vector2i& offset, const Vector2i& extent)
{
    const VkRect2D scissor{.offset = {.x = offset.x, .y = offset.y},
                           .extent = {.width = static_cast<u32>(extent.x), .height = static_cast<u32>(extent.y)}};
    vkCmdSetScissor(m_native_command_buffer, 0, 1, &scissor);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdSetDepthBias(f32 constant_factor, f32 clamp, f32 slope_factor)
{
    // A non-zero clamp is a feature, not a value the driver quietly ignores.
    if (clamp != 0.0f && !m_device->GetFeatures().depth_bias_clamp)
    {
        RNDR_LOG_ERROR("Forge: clamping the depth bias needs DeviceFeatures::depth_bias_clamp");
        return ErrorCode::InvalidArgument;
    }
    vkCmdSetDepthBias(m_native_command_buffer, constant_factor, clamp, slope_factor);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdSetStencilReference(u32 reference, StencilFaceBits faces)
{
    vkCmdSetStencilReference(m_native_command_buffer, static_cast<VkStencilFaceFlags>(faces), reference);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdSetStencilCompareMask(u32 compare_mask, StencilFaceBits faces)
{
    vkCmdSetStencilCompareMask(m_native_command_buffer, static_cast<VkStencilFaceFlags>(faces), compare_mask);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdSetStencilWriteMask(u32 write_mask, StencilFaceBits faces)
{
    vkCmdSetStencilWriteMask(m_native_command_buffer, static_cast<VkStencilFaceFlags>(faces), write_mask);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdSetLineWidth(f32 width)
{
    // One is the only width every device draws; anything else is the wide_lines feature.
    if (width != 1.0f && !m_device->GetFeatures().wide_lines)
    {
        RNDR_LOG_ERROR("Forge: a line width other than one needs DeviceFeatures::wide_lines");
        return ErrorCode::InvalidArgument;
    }
    vkCmdSetLineWidth(m_native_command_buffer, width);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBindVertexBuffer(const Buffer& buffer, u32 binding, u64 offset)
{
    const VkBuffer native_buffer = buffer.GetNativeBuffer();
    vkCmdBindVertexBuffers(m_native_command_buffer, binding, 1, &native_buffer, &offset);
    return ErrorCode::Success;
}

static Opal::Optional<VkIndexType> ToVkIndexType(Rndr::IndexSize index_size)
{
    switch (index_size)
    {
        case Rndr::IndexSize::uint8:
            return Opal::Optional<VkIndexType>(VK_INDEX_TYPE_UINT8_KHR);
        case Rndr::IndexSize::uint16:
            return Opal::Optional<VkIndexType>(VK_INDEX_TYPE_UINT16);
        case Rndr::IndexSize::uint32:
            return Opal::Optional<VkIndexType>(VK_INDEX_TYPE_UINT32);
        default:
            return {};
    }
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBindIndexBuffer(const Buffer& buffer, u64 offset, IndexSize index_size)
{
    // The uint8 index type belongs to an extension, but vkCmdBindIndexBuffer takes it as a plain enum value
    // rather than through a function the loader would only hand out with the extension on. A device that did
    // not enable it is handed an index type it never agreed to, which is the mistake CmdDrawMeshTasks makes
    // in the other direction, so it is caught here rather than left to the validation layer.
    if (index_size == IndexSize::uint8 && !m_device->GetFeatures().index_type_uint8)
    {
        RNDR_LOG_ERROR("Forge: an 8-bit index buffer needs the device created with DeviceFeatures::index_type_uint8");
        return ErrorCode::InvalidArgument;
    }
    RNDR_FORGE_TRANSLATE(index_type, ToVkIndexType(index_size), "the index size");
    vkCmdBindIndexBuffer(m_native_command_buffer, buffer.GetNativeBuffer(), offset, index_type);
    return ErrorCode::Success;
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

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBindPipeline(const Pipeline& pipeline)
{
    vkCmdBindPipeline(m_native_command_buffer, pipeline.GetBindPoint(), pipeline.GetNativePipeline());
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& descriptor_set,
                                                                 u32 first_set)
{
    const VkDescriptorSet native_set = descriptor_set.GetNativeDescriptorSet();
    vkCmdBindDescriptorSets(m_native_command_buffer, pipeline.GetBindPoint(), pipeline.GetNativePipelineLayout(), first_set, 1, &native_set,
                            0, nullptr);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBindDescriptorSets(const Pipeline& pipeline,
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
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdPushConstants(const Pipeline& pipeline, ShaderTypeBits shader_stages,
                                                             Opal::ArrayView<const u8> data, u32 offset)
{
    vkCmdPushConstants(m_native_command_buffer, pipeline.GetNativePipelineLayout(), ToVkShaderStageFlags(shader_stages), offset,
                       static_cast<u32>(data.GetSize()), data.GetData());
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdDrawIndexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset,
                                                           u32 first_instance)
{
    vkCmdDrawIndexed(m_native_command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
    return ErrorCode::Success;
}

/**
 * The checks every indirect command shares: the buffer has to allow indirect use, the offset has to be
 * aligned, and every command the device will read has to fit inside the buffer.
 */
static Rndr::ErrorCode ValidateIndirectRange(const Rndr::Forge::Buffer& buffer, Rndr::u64 offset, Rndr::u32 count, Rndr::u32 stride,
                                             Rndr::u64 command_size, const char* what)
{
    using namespace Rndr;
    if (!(buffer.GetDesc().usage & Forge::BufferUsageBits::IndirectBuffer))
    {
        RNDR_LOG_ERROR("Forge: {} needs a buffer created with BufferUsageBits::IndirectBuffer", what);
        return ErrorCode::InvalidArgument;
    }
    if (offset % 4 != 0)
    {
        RNDR_LOG_ERROR("Forge: the {} offset must be a multiple of 4, and is {}", what, offset);
        return ErrorCode::InvalidArgument;
    }
    if (count == 0)
    {
        return ErrorCode::Success;
    }
    if (count > 1 && (stride % 4 != 0 || stride < command_size))
    {
        RNDR_LOG_ERROR("Forge: the {} stride must be a multiple of 4 and at least one command long, and is {}", what, stride);
        return ErrorCode::InvalidArgument;
    }
    // Written so that neither the offset nor the span of the commands can overflow the sum and pass, the way
    // Buffer::Update checks it. The last command starts at count - 1 strides in, so only that many are counted.
    const u64 buffer_size = buffer.GetSize();
    const u64 span_before_last = static_cast<u64>(count - 1) * stride;
    if (offset > buffer_size || span_before_last > buffer_size - offset || command_size > buffer_size - offset - span_before_last)
    {
        RNDR_LOG_ERROR("Forge: {} reaches past the end of the buffer", what);
        return ErrorCode::OutOfBounds;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdDraw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance)
{
    vkCmdDraw(m_native_command_buffer, vertex_count, instance_count, first_vertex, first_instance);
    return ErrorCode::Success;
}

/** More than one command in one indirect draw is a feature rather than something every device can do. */
static Rndr::ErrorCode ValidateIndirectDrawCount(const Rndr::Forge::Device& device, Rndr::u32 draw_count, const char* what)
{
    if (draw_count > 1 && !device.GetFeatures().multi_draw_indirect)
    {
        RNDR_LOG_ERROR("Forge: {} of more than one command needs the device created with DeviceFeatures::multi_draw_indirect", what);
        return Rndr::ErrorCode::InvalidArgument;
    }
    return Rndr::ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdDrawIndirect(const Buffer& buffer, u64 offset, u32 draw_count, u32 stride)
{
    RNDR_FORGE_CHECK(ValidateIndirectRange(buffer, offset, draw_count, stride, sizeof(DrawIndirectCommand), "Indirect draw"));
    RNDR_FORGE_CHECK(ValidateIndirectDrawCount(*m_device, draw_count, "Indirect draw"));
    vkCmdDrawIndirect(m_native_command_buffer, buffer.GetNativeBuffer(), offset, draw_count, stride);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdDrawIndexedIndirect(const Buffer& buffer, u64 offset, u32 draw_count, u32 stride)
{
    RNDR_FORGE_CHECK(
        ValidateIndirectRange(buffer, offset, draw_count, stride, sizeof(DrawIndexedIndirectCommand), "Indirect indexed draw"));
    RNDR_FORGE_CHECK(ValidateIndirectDrawCount(*m_device, draw_count, "Indirect indexed draw"));
    vkCmdDrawIndexedIndirect(m_native_command_buffer, buffer.GetNativeBuffer(), offset, draw_count, stride);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdDrawMeshTasks(u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
    // The loader hands out a callable trampoline whether or not the device enabled the extension, so a null
    // check does not catch this - calling it without the extension is an access violation, not a failure.
    if (!m_device->IsExtensionEnabled(VK_EXT_MESH_SHADER_EXTENSION_NAME) || vkCmdDrawMeshTasksEXT == nullptr)
    {
        RNDR_LOG_ERROR("Forge: mesh shader drawing needs VK_EXT_mesh_shader, which the device did not enable");
        return ErrorCode::InvalidArgument;
    }
    vkCmdDrawMeshTasksEXT(m_native_command_buffer, group_count_x, group_count_y, group_count_z);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdDispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
    vkCmdDispatch(m_native_command_buffer, group_count_x, group_count_y, group_count_z);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdDispatchIndirect(const Buffer& buffer, u64 offset)
{
    RNDR_FORGE_CHECK(
        ValidateIndirectRange(buffer, offset, 1, sizeof(DispatchIndirectCommand), sizeof(DispatchIndirectCommand), "Indirect dispatch"));
    vkCmdDispatchIndirect(m_native_command_buffer, buffer.GetNativeBuffer(), offset);
    return ErrorCode::Success;
}

namespace
{
/**
 * Whether a label can be recorded at all. The loader hands out a callable trampoline for a debug utils
 * command whether or not the instance enabled the extension, so asking the device is what keeps this from
 * being an access violation - the same trap CmdDrawMeshTasks hits.
 *
 * All three label commands go through this one answer, which is what keeps a region balanced: a build
 * without the extension skips the begin and the end together rather than one of them.
 */
bool AreDebugLabelsUsable(const Rndr::Forge::Device& device)
{
    return device.AreDebugUtilsEnabled() && vkCmdBeginDebugUtilsLabelEXT != nullptr && vkCmdEndDebugUtilsLabelEXT != nullptr &&
           vkCmdInsertDebugUtilsLabelEXT != nullptr;
}

VkDebugUtilsLabelEXT ToVkLabel(const Opal::StringUtf8& name, const Rndr::Vector4f& color)
{
    return {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pLabelName = reinterpret_cast<const char*>(name.GetData()),
            .color = {color.x, color.y, color.z, color.w}};
}
}  // namespace

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdBeginDebugLabel(const Opal::StringUtf8& name, const Vector4f& color)
{
    if (!AreDebugLabelsUsable(*m_device))
    {
        return ErrorCode::Success;
    }
    const VkDebugUtilsLabelEXT label = ToVkLabel(name, color);
    vkCmdBeginDebugUtilsLabelEXT(m_native_command_buffer, &label);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdEndDebugLabel()
{
    if (!AreDebugLabelsUsable(*m_device))
    {
        return ErrorCode::Success;
    }
    vkCmdEndDebugUtilsLabelEXT(m_native_command_buffer);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdInsertDebugLabel(const Opal::StringUtf8& name, const Vector4f& color)
{
    if (!AreDebugLabelsUsable(*m_device))
    {
        return ErrorCode::Success;
    }
    const VkDebugUtilsLabelEXT label = ToVkLabel(name, color);
    vkCmdInsertDebugUtilsLabelEXT(m_native_command_buffer, &label);
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdResetQueryPool(const TimestampQueryPool& query_pool, u32 first_query, u32 query_count)
{
    const Opal::Expected<u32, ErrorCode> resolved_count = query_pool.ResolveQueryRange(first_query, query_count, "Resetting");
    if (!resolved_count.HasValue())
    {
        return resolved_count.GetError();
    }
    vkCmdResetQueryPool(m_native_command_buffer, query_pool.GetNativeQueryPool(), first_query, resolved_count.GetValue());
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CommandBuffer::CmdWriteTimestamp(const TimestampQueryPool& query_pool, u32 query_index,
                                                              PipelineStageBits stage)
{
    const Opal::Expected<u32, ErrorCode> resolved = query_pool.ResolveQueryRange(query_index, 1, "Writing a timestamp into");
    if (!resolved.HasValue())
    {
        return resolved.GetError();
    }
    // vkCmdWriteTimestamp2 takes one stage, not a mask: the tick is written once every previously submitted
    // command has reached that one stage, and there is no defined moment for "reached either of two".
    const u64 stage_bits = static_cast<u64>(stage);
    if (stage_bits == 0 || (stage_bits & (stage_bits - 1)) != 0)
    {
        RNDR_LOG_ERROR("Forge: a timestamp has to name exactly one pipeline stage");
        return ErrorCode::InvalidArgument;
    }
    vkCmdWriteTimestamp2(m_native_command_buffer, static_cast<VkPipelineStageFlags2>(stage), query_pool.GetNativeQueryPool(), query_index);
    return ErrorCode::Success;
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
