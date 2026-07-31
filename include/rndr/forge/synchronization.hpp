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

struct ImageBarrier : Opal::ClonableBase<ImageBarrier>
{
    PipelineStageBits stages_must_finish = PipelineStageBits::None;
    PipelineStageAccessBits stages_must_finish_access = PipelineStageAccessBits::None;
    PipelineStageBits before_stages_start = PipelineStageBits::None;
    PipelineStageAccessBits before_stages_start_access = PipelineStageAccessBits::None;
    ImageLayout old_layout = ImageLayout::Undefined;
    ImageLayout new_layout = ImageLayout::Undefined;
    Opal::Ref<const Texture> image;
    /** The whole texture by default - every mip level, every array layer, the aspect of its format. */
    ImageSubresourceRange subresource_range;

    OPAL_CLONE_FIELDS(stages_must_finish, stages_must_finish_access, before_stages_start, before_stages_start_access, old_layout,
                      new_layout, image, subresource_range);

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
};

}  // namespace Rndr::Forge
