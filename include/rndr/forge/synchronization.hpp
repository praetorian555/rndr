#pragma once

#include "volk/volk.h"

#include "opal/container/array-view.h"
#include "opal/container/ref.h"

#include "rndr/forge/texture.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"

namespace Rndr::Forge
{

// Used for synchronization between CPU and GPU.
class Fence
{
public:
    static constexpr u64 k_infinite_wait = UINT64_MAX;

    Fence() = default;
    Fence(const Device& device, bool create_signaled);
    ~Fence();

    void Destroy();

    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;

    Fence(Fence&& other) noexcept;
    Fence& operator=(Fence&& other) noexcept;

    [[nodiscard]] bool IsValid() const { return m_fence != VK_NULL_HANDLE; }
    [[nodiscard]] VkFence GetNativeFence() const { return m_fence; }

    void Wait(u64 timeout = k_infinite_wait) const;

    void Reset() const;

    static void WaitForAll(Opal::ArrayView<const Fence> fences, u64 timeout = k_infinite_wait);

private:
    VkFence m_fence = VK_NULL_HANDLE;
    Opal::Ref<const Device> m_device;
};

// Used for synchronization on the GPU.
class Semaphore
{
public:
    Semaphore() = default;
    explicit Semaphore(const Device& device);
    ~Semaphore();

    void Destroy();

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    Semaphore(Semaphore&& other) noexcept;
    Semaphore& operator=(Semaphore&& other) noexcept;

    [[nodiscard]] bool IsValid() const { return m_semaphore != VK_NULL_HANDLE; }
    [[nodiscard]] VkSemaphore GetNativeSemaphore() const { return m_semaphore; }

private:
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    Opal::Ref<const Device> m_device;
};

/**
 * A dependency between two pieces of work that covers all memory, without naming a resource. Use it when the
 * work on either side touches more resources than it is worth listing, and a buffer or image barrier when it
 * does not - naming the resource lets the driver narrow what it has to flush.
 */
struct MemoryBarrier
{
    PipelineStageBits stages_must_finish = PipelineStageBits::None;
    PipelineStageAccessBits stages_must_finish_access = PipelineStageAccessBits::None;
    PipelineStageBits before_stages_start = PipelineStageBits::None;
    PipelineStageAccessBits before_stages_start_access = PipelineStageAccessBits::None;
};

/**
 * A dependency on one range of one buffer. Buffers have no layout, so this only orders access: the write of a
 * compute shader against the read of a vertex shader, a transfer against anything that reads what it wrote.
 */
struct BufferBarrier : Opal::ClonableBase<BufferBarrier>
{
    PipelineStageBits stages_must_finish = PipelineStageBits::None;
    PipelineStageAccessBits stages_must_finish_access = PipelineStageAccessBits::None;
    PipelineStageBits before_stages_start = PipelineStageBits::None;
    PipelineStageAccessBits before_stages_start_access = PipelineStageAccessBits::None;
    /**
     * Queue families the resource is handed between. Both ignored by default, which means no transfer and is
     * what a resource used on one queue wants. A transfer is two barriers with the same pair of families: a
     * release on the source queue and an acquire on the destination one, in that order.
     */
    u32 source_queue_family = k_ignored_queue_family;
    u32 destination_queue_family = k_ignored_queue_family;
    Opal::Ref<const Buffer> buffer;
    u64 offset = 0;
    /** The whole buffer from the offset on by default. */
    u64 size = k_whole_buffer;

    OPAL_CLONE_FIELDS(stages_must_finish, stages_must_finish_access, before_stages_start, before_stages_start_access,
                      source_queue_family, destination_queue_family, buffer, offset, size);

    /**
     * A write in one set of stages, followed by a read in another. The two common cases are a compute shader
     * feeding the vertex input stage and a transfer feeding a shader.
     */
    [[nodiscard]] static BufferBarrier WriteThenRead(const Buffer& buffer, PipelineStageBits writer, PipelineStageBits reader);

    /** A read in one set of stages, followed by a write in another, so the write does not run ahead of it. */
    [[nodiscard]] static BufferBarrier ReadThenWrite(const Buffer& buffer, PipelineStageBits reader, PipelineStageBits writer);
};

struct ImageBarrier : Opal::ClonableBase<ImageBarrier>
{
    PipelineStageBits stages_must_finish = PipelineStageBits::None;
    PipelineStageAccessBits stages_must_finish_access = PipelineStageAccessBits::None;
    PipelineStageBits before_stages_start = PipelineStageBits::None;
    PipelineStageAccessBits before_stages_start_access = PipelineStageAccessBits::None;
    ImageLayout old_layout = ImageLayout::Undefined;
    ImageLayout new_layout = ImageLayout::Undefined;
    /**
     * Queue families the resource is handed between. Both ignored by default, which means no transfer and is
     * what a resource used on one queue wants. A transfer is two barriers with the same pair of families: a
     * release on the source queue and an acquire on the destination one, in that order.
     */
    u32 source_queue_family = k_ignored_queue_family;
    u32 destination_queue_family = k_ignored_queue_family;
    Opal::Ref<const Texture> image;
    /** The whole texture by default - every mip level, every array layer, the aspect of its format. */
    ImageSubresourceRange subresource_range;

    OPAL_CLONE_FIELDS(stages_must_finish, stages_must_finish_access, before_stages_start, before_stages_start_access, old_layout,
                      new_layout, source_queue_family, destination_queue_family, image, subresource_range);

    /**
     * The standard transitions, spelled out. Each covers the whole texture and picks the stages and the
     * access from what the texture is about to be used for, so only the layout it is coming from is left to
     * the caller. Where that layout has an obvious answer it is the default; where getting it wrong would
     * throw away the contents of the texture, it is not.
     *
     * ImageLayout::Undefined as the old layout discards whatever the texture holds, which is what a color
     * attachment that is cleared at the start of the frame wants and what a texture that is about to be read
     * does not.
     */

    /** Rendered into as a color attachment. */
    [[nodiscard]] static ImageBarrier ToColorAttachment(const Texture& texture, ImageLayout old_layout = ImageLayout::Undefined);

    /** Rendered into as a depth or stencil attachment. */
    [[nodiscard]] static ImageBarrier ToDepthStencilAttachment(const Texture& texture, ImageLayout old_layout = ImageLayout::Undefined);

    /** Sampled in a shader. The reader defaults to the fragment stage. */
    [[nodiscard]] static ImageBarrier ToShaderRead(const Texture& texture, ImageLayout old_layout,
                                                   PipelineStageBits reader = PipelineStageBits::FragmentShader);

    /** Written by a transfer command, which is how a texture is uploaded. */
    [[nodiscard]] static ImageBarrier ToTransferDestination(const Texture& texture, ImageLayout old_layout = ImageLayout::Undefined);

    /** Read by a transfer command, which is how a texture is read back or has its mips generated. */
    [[nodiscard]] static ImageBarrier ToTransferSource(const Texture& texture, ImageLayout old_layout);

    /** Handed to the presentation engine. */
    [[nodiscard]] static ImageBarrier ToPresent(const Texture& texture, ImageLayout old_layout = ImageLayout::ColorAttachment);

    /**
     * The preset for a destination layout that is not known while writing the call - a function handed the
     * layout to leave a texture in dispatches through this rather than spelling the switch out again. Throws
     * for a layout that has no preset above.
     */
    [[nodiscard]] static ImageBarrier To(const Texture& texture, ImageLayout old_layout, ImageLayout new_layout);
};

}  // namespace Rndr::Forge
