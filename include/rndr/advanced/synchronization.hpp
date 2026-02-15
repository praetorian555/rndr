#pragma once

#include "volk/volk.h"

#include "opal/container/ref.h"
#include "opal/container/array-view.h"

#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"
#include "rndr/advanced/advanced-texture.hpp"

namespace Rndr
{

// Used for synchronization between CPU and GPU.
class AdvancedFence
{
public:
    static constexpr u64 k_infinite_wait = UINT64_MAX;

    AdvancedFence() = default;
    AdvancedFence(const class AdvancedDevice& device, bool create_signaled);
    ~AdvancedFence();

    void Destroy();

    AdvancedFence(const AdvancedFence&) = delete;
    AdvancedFence& operator=(const AdvancedFence&) = delete;

    AdvancedFence(AdvancedFence&& other) noexcept;
    AdvancedFence& operator=(AdvancedFence&& other) noexcept;

    [[nodiscard]] VkFence GetNativeFence() const { return m_fence; }

    void Wait(u64 timeout = k_infinite_wait) const;

    void Reset() const;

    static void WaitForAll(Opal::ArrayView<AdvancedFence> fences, u64 timeout = k_infinite_wait);

private:
    VkFence m_fence = VK_NULL_HANDLE;
    Opal::Ref<const class AdvancedDevice> m_device;
};

// Used for synchronization on the GPU.
class AdvancedSemaphore
{
public:
    AdvancedSemaphore() = default;
    explicit AdvancedSemaphore(const class AdvancedDevice& device);
    ~AdvancedSemaphore();

    void Destroy();

    AdvancedSemaphore(const AdvancedSemaphore&) = delete;
    AdvancedSemaphore& operator=(const AdvancedSemaphore&) = delete;

    AdvancedSemaphore(AdvancedSemaphore&& other) noexcept;
    AdvancedSemaphore& operator=(AdvancedSemaphore&& other) noexcept;

    [[nodiscard]] VkSemaphore GetNativeSemaphore() const { return m_semaphore; }

private:
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    Opal::Ref<const class AdvancedDevice> m_device;
};

struct AdvancedImageBarrier
{
    PipelineStageBits stages_must_finish;
    PipelineStageAccessBits stages_must_finish_access;
    PipelineStageBits before_stages_start;
    PipelineStageAccessBits before_stages_start_access;
    ImageLayout old_layout;
    ImageLayout new_layout;
    Opal::Ref<const class AdvancedTexture> image;
    ImageSubresourceRange subresource_range;
};

}  // namespace Rndr
