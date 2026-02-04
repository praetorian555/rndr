#include "rndr/advanced/synchronization.hpp"

#include "rndr/advanced/device.hpp"

Rndr::AdvancedFence::AdvancedFence(const class AdvancedDevice& device, bool create_signaled) : m_device(device)
{
    const VkFenceCreateInfo fence_create_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                                 .flags = create_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u};

    const VkResult result = vkCreateFence(device.GetNativeDevice(), &fence_create_info, nullptr, &m_fence);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create a fence");
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

Rndr::AdvancedSemaphore::AdvancedSemaphore(const AdvancedDevice& device) : m_device(device)
{
    const VkSemaphoreCreateInfo semaphore_create_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    const VkResult result = vkCreateSemaphore(device.GetNativeDevice(), &semaphore_create_info, nullptr, &m_semaphore);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create a present semaphore!");
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
