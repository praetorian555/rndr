#include "rndr/forge/synchronization.hpp"

#include <mutex>

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-exception.hpp"

Rndr::Forge::Fence::Fence(const Device& device, bool create_signaled) : m_device(device)
{
    const VkFenceCreateInfo fence_create_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                                 .flags = create_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u};

    const VkResult result = vkCreateFence(device.GetNativeDevice(), &fence_create_info, nullptr, &m_fence);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateFence");
    }
}

Rndr::Forge::Fence::~Fence()
{
    Destroy();
}

void Rndr::Forge::Fence::Destroy()
{
    if (m_fence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_device->GetNativeDevice(), m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }
}

Rndr::Forge::Fence::Fence(Fence&& other) noexcept : m_device(std::move(other.m_device)), m_fence(other.m_fence)
{
    other.m_fence = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::Forge::Fence& Rndr::Forge::Fence::operator=(Fence&& other) noexcept
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

void Rndr::Forge::Fence::Wait(u64 timeout) const
{
    const VkResult result = vkWaitForFences(m_device->GetNativeDevice(), 1, &m_fence, VK_TRUE, timeout);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkWaitForFences");
    }
}

void Rndr::Forge::Fence::Reset() const
{
    const VkResult reset_result = vkResetFences(m_device->GetNativeDevice(), 1, &m_fence);
    if (reset_result != VK_SUCCESS)
    {
        throw VulkanException(reset_result, "vkResetFences");
    }
}

void Rndr::Forge::Fence::WaitForAll(Opal::ArrayView<Fence> fences, u64 timeout)
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

Rndr::Forge::Semaphore::Semaphore(const Device& device) : m_device(device)
{
    const VkSemaphoreCreateInfo semaphore_create_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    const VkResult result = vkCreateSemaphore(device.GetNativeDevice(), &semaphore_create_info, nullptr, &m_semaphore);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateSemaphore");
    }
}

Rndr::Forge::Semaphore::~Semaphore()
{
    Destroy();
}

void Rndr::Forge::Semaphore::Destroy()
{
    if (m_semaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device->GetNativeDevice(), m_semaphore, nullptr);
        m_semaphore = VK_NULL_HANDLE;
    }
}

Rndr::Forge::Semaphore::Semaphore(Semaphore&& other) noexcept
    : m_device(std::move(other.m_device)), m_semaphore(other.m_semaphore)
{
    other.m_device = nullptr;
    other.m_semaphore = VK_NULL_HANDLE;
}

Rndr::Forge::Semaphore& Rndr::Forge::Semaphore::operator=(Semaphore&& other) noexcept
{
    if (this != &other)
    {
        m_device = std::move(other.m_device);
        m_semaphore = other.m_semaphore;
        other.m_semaphore = VK_NULL_HANDLE;
    }
    return *this;
}
