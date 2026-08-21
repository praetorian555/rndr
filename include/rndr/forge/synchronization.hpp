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

/** A timeout that never expires, which is what the blocking waits below pass. */
static constexpr u64 k_infinite_wait = UINT64_MAX;

// Used for synchronization between CPU and GPU.
class Fence
{
public:
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

    /** Block until the fence is signalled. */
    void Wait() const;

    /**
     * The same wait, given up on after the timeout in nanoseconds.
     * @return True once the fence is signalled, false when the timeout expired first, which is not a failure.
     */
    [[nodiscard]] bool TryWait(u64 timeout) const;

    void Reset() const;

    static void WaitForAll(Opal::ArrayView<const Fence> fences);

    /** WaitForAll with a timeout, answering the way TryWait does. */
    [[nodiscard]] static bool TryWaitForAll(Opal::ArrayView<const Fence> fences, u64 timeout);

private:
    VkFence m_fence = VK_NULL_HANDLE;
    Opal::Ref<const Device> m_device;
};

enum class SemaphoreType : u8
{
    Binary,    // Signalled or not. Only a device wait can consume it, and consuming it clears it.
    Timeline,  // A counter that only rises. Host and device both wait on it and both raise it.
};

struct SemaphoreDesc
{
    SemaphoreType type = SemaphoreType::Binary;
    /** Timeline only. The count it starts at; a wait for that value or less is satisfied at once. */
    u64 initial_value = 0;
};

/** One semaphore and the count to wait for, for the batched host-side wait. */
struct SemaphoreWait
{
    Opal::Ref<const Semaphore> semaphore;
    u64 value = 0;
};

// Used for synchronization on the GPU, and for a timeline, between the GPU and the host as well.
class Semaphore
{
public:
    Semaphore() = default;
    explicit Semaphore(const Device& device, const SemaphoreDesc& desc = {});
    ~Semaphore();

    void Destroy();

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    Semaphore(Semaphore&& other) noexcept;
    Semaphore& operator=(Semaphore&& other) noexcept;

    [[nodiscard]] bool IsValid() const { return m_semaphore != VK_NULL_HANDLE; }
    [[nodiscard]] VkSemaphore GetNativeSemaphore() const { return m_semaphore; }

    [[nodiscard]] SemaphoreType GetType() const { return m_type; }
    [[nodiscard]] bool IsTimeline() const { return m_type == SemaphoreType::Timeline; }

    /**
     * The four calls below are the host side of a timeline, and all of them throw on a binary semaphore -
     * only a device wait can consume one of those, so there is nothing here for the host to do with it.
     */

    /** Block until the count reaches the given value. A value already reached returns at once. */
    void Wait(u64 value) const;

    /**
     * The same wait, given up on after the timeout in nanoseconds.
     * @return True once the count reached the value, false when the timeout expired first, which is not a failure.
     */
    [[nodiscard]] bool TryWait(u64 value, u64 timeout) const;

    /** Raise the count from the host. The value has to be above the current one. */
    void Signal(u64 value) const;

    /** The count as it stands. A device signal may have raised it again by the time this returns. */
    [[nodiscard]] u64 GetValue() const;

    /**
     * One wait over many semaphores, which returns once every one of them has reached its value. They all
     * have to belong to the same device, since one wait is one call into one device.
     */
    static void WaitForAll(Opal::ArrayView<const SemaphoreWait> waits);

    /** WaitForAll with a timeout, answering the way TryWait does. */
    [[nodiscard]] static bool TryWaitForAll(Opal::ArrayView<const SemaphoreWait> waits, u64 timeout);

private:
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    Opal::Ref<const Device> m_device;
    SemaphoreType m_type = SemaphoreType::Binary;
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
    Opal::Ref<Texture> image;
    /** The whole texture by default - every mip level, every array layer, the aspect of its format. */
    ImageSubresourceRange subresource_range;

    OPAL_CLONE_FIELDS(stages_must_finish, stages_must_finish_access, before_stages_start, before_stages_start_access, old_layout,
                      new_layout, source_queue_family, destination_queue_family, image, subresource_range);

    /**
     * The standard transitions, spelled out. Each covers the whole texture and picks the stages and the
     * access from what the texture is about to be used for.
     *
     * Every one comes in two forms. The short one takes the layout the texture is coming from off the texture
     * itself, which is what Texture::GetCurrentLayout tracks and what almost every caller wants; it throws
     * when the levels of the texture are not all in one layout, since there is then no honest answer. The long
     * one is told that layout, for the caller that knows better than the tracker - a barrier over part of a
     * texture, or a deliberate discard.
     *
     * ImageLayout::Undefined as the old layout is that discard: it throws away whatever the texture holds,
     * which is what a color attachment cleared at the start of the frame wants and what a texture that is
     * about to be read does not.
     */

    /** Rendered into as a color attachment. */
    [[nodiscard]] static ImageBarrier ToColorAttachment(Texture& texture);
    [[nodiscard]] static ImageBarrier ToColorAttachment(Texture& texture, ImageLayout old_layout);

    /** Rendered into as a depth or stencil attachment. */
    [[nodiscard]] static ImageBarrier ToDepthStencilAttachment(Texture& texture);
    [[nodiscard]] static ImageBarrier ToDepthStencilAttachment(Texture& texture, ImageLayout old_layout);

    /** Sampled in a shader. The reader defaults to the fragment stage. */
    [[nodiscard]] static ImageBarrier ToShaderRead(Texture& texture, PipelineStageBits reader = PipelineStageBits::FragmentShader);
    [[nodiscard]] static ImageBarrier ToShaderRead(Texture& texture, ImageLayout old_layout,
                                                   PipelineStageBits reader = PipelineStageBits::FragmentShader);

    /** Written by a transfer command, which is how a texture is uploaded. */
    [[nodiscard]] static ImageBarrier ToTransferDestination(Texture& texture);
    [[nodiscard]] static ImageBarrier ToTransferDestination(Texture& texture, ImageLayout old_layout);

    /** Read by a transfer command, which is how a texture is read back or has its mips generated. */
    [[nodiscard]] static ImageBarrier ToTransferSource(Texture& texture);
    [[nodiscard]] static ImageBarrier ToTransferSource(Texture& texture, ImageLayout old_layout);

    /** Handed to the presentation engine. */
    [[nodiscard]] static ImageBarrier ToPresent(Texture& texture);
    [[nodiscard]] static ImageBarrier ToPresent(Texture& texture, ImageLayout old_layout);

    /**
     * The preset for a destination layout that is not known while writing the call - a function handed the
     * layout to leave a texture in dispatches through this rather than spelling the switch out again. Throws
     * for a layout that has no preset above.
     *
     * This one has no short form, and deliberately: with both, dropping an argument from a three argument
     * call would leave a two argument one that compiles and means the opposite, since the layout in the
     * middle is the source and the layout at the end is the destination. CommandBuffer::CmdTransition is the
     * short form - it is this preset over the tracked layout, recorded.
     */
    [[nodiscard]] static ImageBarrier To(Texture& texture, ImageLayout old_layout, ImageLayout new_layout);
};

}  // namespace Rndr::Forge
