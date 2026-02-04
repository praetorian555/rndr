#pragma once

#include "volk/volk.h"

#include "opal/container/ref.h"

#include "rndr/types.hpp"

namespace Rndr
{

// Used for synchronization between CPU and GPU.
class AdvancedFence
{
public:
    AdvancedFence() = default;
    AdvancedFence(const class AdvancedDevice& device, bool create_signaled);
    ~AdvancedFence();

    void Destroy();

    AdvancedFence(const AdvancedFence&) = delete;
    AdvancedFence& operator=(const AdvancedFence&) = delete;

    AdvancedFence(AdvancedFence&& other) noexcept;
    AdvancedFence& operator=(AdvancedFence&& other) noexcept;

    [[nodiscard]] VkFence GetNativeFence() const { return m_fence; }

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

}  // namespace Rndr
