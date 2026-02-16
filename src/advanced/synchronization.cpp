#include "rndr/advanced/synchronization.hpp"

#include <mutex>

#include "rndr/advanced/device.hpp"
#include "rndr/advanced/vulkan-exception.hpp"

Rndr::AdvancedFence::AdvancedFence(const class AdvancedDevice& device, bool create_signaled) : m_device(device)
{
    const VkFenceCreateInfo fence_create_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                                 .flags = create_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u};

    const VkResult result = vkCreateFence(device.GetNativeDevice(), &fence_create_info, nullptr, &m_fence);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateFence");
    }
}

Rndr::AdvancedFence::~AdvancedFence()
{
    Destroy();
}

void Rndr::AdvancedFence::Destroy()
{
    if (m_fence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_device->GetNativeDevice(), m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }
}

Rndr::AdvancedFence::AdvancedFence(AdvancedFence&& other) noexcept : m_device(std::move(other.m_device)), m_fence(other.m_fence)
{
    other.m_fence = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::AdvancedFence& Rndr::AdvancedFence::operator=(AdvancedFence&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_fence = other.m_fence;
        other.m_fence = VK_NULL_HANDLE;
    }
    return *this;
}

void Rndr::AdvancedFence::Wait(u64 timeout) const
{
    const VkResult result = vkWaitForFences(m_device->GetNativeDevice(), 1, &m_fence, VK_TRUE, timeout);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkWaitForFences");
    }
}

void Rndr::AdvancedFence::Reset() const
{
    const VkResult reset_result = vkResetFences(m_device->GetNativeDevice(), 1, &m_fence);
    if (reset_result != VK_SUCCESS)
    {
        throw VulkanException(reset_result, "vkResetFences");
    }
}

void Rndr::AdvancedFence::WaitForAll(Opal::ArrayView<AdvancedFence> fences, u64 timeout)
{
    if (fences.empty())
    {
        return;
    }
    Opal::DynamicArray<VkFence> native_fences(fences.GetSize(), Opal::GetScratchAllocator());
    for (i32 i = 0; i < fences.GetSize(); ++i)
    {
        native_fences[i] = fences[i].GetNativeFence();
    }
    const VkResult wait_result =
        vkWaitForFences(fences[0].m_device->GetNativeDevice(), static_cast<u32>(native_fences.GetSize()), native_fences.GetData(), VK_TRUE, timeout);
    if (wait_result != VK_SUCCESS)
    {
        throw VulkanException(wait_result, "vkWaitForFences");
    }
}

Rndr::AdvancedSemaphore::AdvancedSemaphore(const AdvancedDevice& device) : m_device(device)
{
    const VkSemaphoreCreateInfo semaphore_create_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    const VkResult result = vkCreateSemaphore(device.GetNativeDevice(), &semaphore_create_info, nullptr, &m_semaphore);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateSemaphore");
    }
}

Rndr::AdvancedSemaphore::~AdvancedSemaphore()
{
    Destroy();
}

void Rndr::AdvancedSemaphore::Destroy()
{
    if (m_semaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device->GetNativeDevice(), m_semaphore, nullptr);
        m_semaphore = VK_NULL_HANDLE;
    }
}

Rndr::AdvancedSemaphore::AdvancedSemaphore(AdvancedSemaphore&& other) noexcept
    : m_device(std::move(other.m_device)), m_semaphore(other.m_semaphore)
{
    other.m_device = nullptr;
    other.m_semaphore = VK_NULL_HANDLE;
}

Rndr::AdvancedSemaphore& Rndr::AdvancedSemaphore::operator=(AdvancedSemaphore&& other) noexcept
{
    if (this != &other)
    {
        m_device = std::move(other.m_device);
        m_semaphore = other.m_semaphore;
        other.m_semaphore = VK_NULL_HANDLE;
    }
    return *this;
}
