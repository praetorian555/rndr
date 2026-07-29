#pragma once

#include "volk/volk.h"

#include "opal/container/array-view.h"
#include "opal/container/ref.h"

#include "rndr/forge/texture.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"

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

    [[nodiscard]] VkFence GetNativeFence() const { return m_fence; }

    void Wait(u64 timeout = k_infinite_wait) const;

    void Reset() const;

    static void WaitForAll(Opal::ArrayView<Fence> fences, u64 timeout = k_infinite_wait);

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

    [[nodiscard]] VkSemaphore GetNativeSemaphore() const { return m_semaphore; }

private:
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    Opal::Ref<const Device> m_device;
};

struct ImageBarrier : Opal::ClonableBase<ImageBarrier>
{
    PipelineStageBits stages_must_finish;
    PipelineStageAccessBits stages_must_finish_access;
    PipelineStageBits before_stages_start;
    PipelineStageAccessBits before_stages_start_access;
    ImageLayout old_layout;
    ImageLayout new_layout;
    Opal::Ref<const Texture> image;
    ImageSubresourceRange subresource_range;

    OPAL_CLONE_FIELDS(stages_must_finish, stages_must_finish_access, before_stages_start, before_stages_start_access, old_layout,
                      new_layout, image, subresource_range);
};

}  // namespace Rndr::Forge
