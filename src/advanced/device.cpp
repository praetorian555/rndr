#include "rndr/advanced/device.hpp"

#define NOMINMAX
#include "vma/vk_mem_alloc.h"

#include "opal/container/hash-set.h"

#include "rndr/advanced/command-buffer.hpp"
#include "rndr/advanced/graphics-context.hpp"
#include "rndr/advanced/swap-chain.hpp"
#include "rndr/advanced/synchronization.hpp"
#include "rndr/advanced/vulkan-exception.hpp"

Opal::DynamicArray<Rndr::u32> Rndr::AdvancedQueueFamilyIndices::GetValidQueueFamilies() const
{
    Opal::HashSet<u32> unique_indices(6, Opal::GetScratchAllocator());
    Opal::DynamicArray<u32> valid_queue_families;
    if (graphics_family != k_invalid_index)
    {
        unique_indices.Insert(graphics_family);
    }
    if (present_family != k_invalid_index)
    {
        unique_indices.Insert(present_family);
    }
    if (transfer_family != k_invalid_index)
    {
        unique_indices.Insert(transfer_family);
    }
    if (compute_family != k_invalid_index)
    {
        unique_indices.Insert(compute_family);
    }
    if (encode_family_index != k_invalid_index)
    {
        unique_indices.Insert(encode_family_index);
    }
    if (decode_family_index != k_invalid_index)
    {
        unique_indices.Insert(decode_family_index);
    }
    for (auto index : unique_indices)
    {
        valid_queue_families.PushBack(index);
    }
    return valid_queue_families;
}

Rndr::u32 Rndr::AdvancedQueueFamilyIndices::GetQueueFamilyIndex(QueueFamily queue_family) const
{
    switch (queue_family)
    {
        case QueueFamily::Graphics:
            return graphics_family;
        case QueueFamily::Present:
            return present_family;
        case QueueFamily::Transfer:
            return transfer_family;
        case QueueFamily::AsyncCompute:
            return compute_family;
        case QueueFamily::Decode:
            return decode_family_index;
        case QueueFamily::Encode:
            return encode_family_index;
        default:
            throw Opal::Exception("Invalid queue family!");
    }
}

Rndr::AdvancedDevice::AdvancedDevice(AdvancedPhysicalDevice physical_device, const AdvancedGraphicsContext& graphics_context,
                                     const AdvancedDeviceDesc& desc)
    : m_desc(desc), m_physical_device(std::move(physical_device))
{
    if (!m_physical_device.IsValid())
    {
        throw Opal::Exception("Physical device is invalid!");
    }

    Opal::DynamicArray<VkDeviceQueueCreateInfo> queue_create_infos;
    CollectQueueFamilies(queue_create_infos);

    Opal::DynamicArray device_extensions(desc.extensions);
    if (desc.surface.IsValid())
    {
        device_extensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    for (const char* extension_name : device_extensions)
    {
        if (!m_physical_device.IsExtensionSupported(extension_name))
        {
            throw Opal::Exception("Device extension not supported!");
        }
    }

    VkPhysicalDeviceVulkan12Features enabled_vk12_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = 1,
        .descriptorBindingVariableDescriptorCount = 1,
        .runtimeDescriptorArray = 1,
        .bufferDeviceAddress = 1};  // So you can get pointer to raw GPU buffer memory and pass it to shader
    const VkPhysicalDeviceVulkan13Features enabled_vk13_features = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                                                                    .pNext = &enabled_vk12_features,
                                                                    .synchronization2 = 1,
                                                                    .dynamicRendering = 1};
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &enabled_vk13_features, create_info.pQueueCreateInfos = queue_create_infos.GetData();
    create_info.queueCreateInfoCount = static_cast<u32>(queue_create_infos.GetSize());
    create_info.pEnabledFeatures = &desc.features;
    create_info.ppEnabledExtensionNames = device_extensions.GetData();
    create_info.enabledExtensionCount = static_cast<u32>(device_extensions.GetSize());

    const VkResult result = vkCreateDevice(m_physical_device.GetNativePhysicalDevice(), &create_info, nullptr, &m_device);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateDevice");
    }

    auto queue_family_indices_array = m_queue_family_indices.GetValidQueueFamilies();
    for (u8 queue_family_idx = 0; queue_family_idx < static_cast<u8>(QueueFamily::EnumCount); ++queue_family_idx)
    {
        const QueueFamily queue_family = static_cast<QueueFamily>(queue_family_idx);
        const u32 queue_family_index = m_queue_family_indices.GetQueueFamilyIndex(queue_family);
        if (queue_family_index == AdvancedQueueFamilyIndices::k_invalid_index)
        {
            continue;
        }
        bool already_present = false;
        for (const auto& pair : m_queue_family_to_queue)
        {
            if (pair.value->GetQueueFamilyIndex() == queue_family_index)
            {
                m_queue_family_to_queue.Insert(queue_family, pair.value.Clone());
                already_present = true;
                break;
            }
        }
        if (already_present)
        {
            continue;
        }
        Opal::SharedPtr<AdvancedDeviceQueue> queue_ptr(Opal::GetDefaultAllocator(), *this, queue_family_index);
        m_queue_family_to_queue.Insert(queue_family, std::move(queue_ptr));
    }

    // Setup GPU allocator
    const VmaVulkanFunctions vk_functions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkCreateImage = vkCreateImage,
    };
    const VmaAllocatorCreateInfo vma_alloc_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = m_physical_device.GetNativePhysicalDevice(),
        .device = m_device,
        .pVulkanFunctions = &vk_functions,
        .instance = graphics_context.GetInstance(),
    };
    const VkResult vma_result = vmaCreateAllocator(&vma_alloc_create_info, &m_gpu_allocator);
    if (vma_result != VK_SUCCESS)
    {
        throw VulkanException(vma_result, "vmaCreateAllocator");
    }
}

namespace
{
Rndr::f32 g_queue_priority = 1.0f;
}

void Rndr::AdvancedDevice::WaitForAll() const 
{
    const VkResult wait_result = vkDeviceWaitIdle(m_device);
    if (wait_result != VK_SUCCESS)
    {
        throw VulkanException(wait_result, "vkDeviceWaitIdle");
    }
}

void Rndr::AdvancedDevice::CollectQueueFamilies(Opal::DynamicArray<VkDeviceQueueCreateInfo>& queue_create_infos)
{
    auto queue_family_index = m_physical_device.GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
    if (queue_family_index.HasValue())
    {
        m_queue_family_indices.graphics_family = queue_family_index.GetValue();
    }
    else
    {
        throw Opal::Exception("No queue with graphics capabilities!");
    }

    if (m_desc.surface.IsValid())
    {
        auto details = m_desc.surface->GetSwapChainSupportDetails(m_physical_device);
        auto present_queue_family_index = m_physical_device.GetPresentQueueFamilyIndex(m_desc.surface);
        if (!present_queue_family_index.HasValue())
        {
            throw Opal::Exception("No queue with present capabilities but surface provided!");
        }
        m_queue_family_indices.present_family = present_queue_family_index.GetValue();
    }

    if (m_desc.use_async_compute_queue)
    {
        queue_family_index = m_physical_device.GetQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT);
        if (queue_family_index.HasValue())
        {
            m_queue_family_indices.compute_family = queue_family_index.GetValue();
        }
        else
        {
            throw Opal::Exception("Async compute queue requested but device does not support it");
        }
    }

    if (m_desc.use_dedicated_transfer_queue)
    {
        queue_family_index =
            m_physical_device.GetQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT |
                                                                             VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_VIDEO_ENCODE_BIT_KHR);
        if (queue_family_index.HasValue())
        {
            m_queue_family_indices.transfer_family = queue_family_index.GetValue();
        }
        else
        {
            throw Opal::Exception("Dedicated transfer queue requested but device does not support it");
        }
    }

    if (m_desc.use_encode_queue)
    {
        queue_family_index = m_physical_device.GetQueueFamilyIndex(VK_QUEUE_VIDEO_ENCODE_BIT_KHR);
        if (queue_family_index.HasValue())
        {
            m_queue_family_indices.encode_family_index = queue_family_index.GetValue();
        }
        else
        {
            throw Opal::Exception("Video encode queue requested but device does not support it");
        }
    }

    if (m_desc.use_decode_queue)
    {
        queue_family_index = m_physical_device.GetQueueFamilyIndex(VK_QUEUE_VIDEO_DECODE_BIT_KHR);
        if (queue_family_index.HasValue())
        {
            m_queue_family_indices.decode_family_index = queue_family_index.GetValue();
        }
        else
        {
            throw Opal::Exception("Video encode queue requested but device does not support it");
        }
    }
    for (const u32 index : m_queue_family_indices.GetValidQueueFamilies())
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = index;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &g_queue_priority;
        queue_create_infos.PushBack(queue_create_info);
    }
}

Rndr::AdvancedDevice::~AdvancedDevice()
{
    Destroy();
}

Rndr::AdvancedDevice::AdvancedDevice(AdvancedDevice&& other) noexcept
    : m_device(other.m_device),
      m_queue_family_to_queue(std::move(other.m_queue_family_to_queue)),
      m_physical_device(std::move(other.m_physical_device)),
      m_desc(std::move(other.m_desc)),
      m_queue_family_indices(other.m_queue_family_indices)
{
    other.m_device = VK_NULL_HANDLE;
    other.m_queue_family_to_queue.Clear();
    other.m_physical_device = {};
    other.m_desc = {};
    other.m_queue_family_indices = {};
}

Rndr::AdvancedDevice& Rndr::AdvancedDevice::operator=(AdvancedDevice&& other) noexcept
{
    Destroy();

    m_device = other.m_device;
    m_queue_family_to_queue = std::move(other.m_queue_family_to_queue);
    m_physical_device = std::move(other.m_physical_device);
    m_desc = std::move(other.m_desc);
    m_queue_family_indices = other.m_queue_family_indices;

    other.m_device = VK_NULL_HANDLE;
    other.m_queue_family_to_queue.Clear();
    other.m_physical_device = {};
    other.m_desc = {};
    other.m_queue_family_indices = {};

    return *this;
}

void Rndr::AdvancedDevice::Destroy()
{
    if (m_gpu_allocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(m_gpu_allocator);
        m_gpu_allocator = VK_NULL_HANDLE;
    }
    m_queue_family_to_queue.Clear();
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    m_physical_device = {};
    m_desc = {};
}

Opal::Ref<Rndr::AdvancedDeviceQueue> Rndr::AdvancedDevice::GetQueue(QueueFamily queue_family)
{
    auto queue_it = m_queue_family_to_queue.Find(queue_family);
    if (queue_it == m_queue_family_to_queue.end())
    {
        throw Opal::Exception("Queue family not supported!");
    }
    return queue_it.GetValue().GetRef();
}

Opal::Ref<const Rndr::AdvancedDeviceQueue> Rndr::AdvancedDevice::GetQueue(QueueFamily queue_family) const
{
    auto queue_it = m_queue_family_to_queue.Find(queue_family);
    if (queue_it == m_queue_family_to_queue.end())
    {
        throw Opal::Exception("Queue family not supported!");
    }
    return queue_it.GetValue().GetRef();
}

Rndr::AdvancedDeviceQueue::~AdvancedDeviceQueue()
{
    Destroy();
}

void Rndr::AdvancedDeviceQueue::Destroy()
{
    m_queue = VK_NULL_HANDLE;
    if (m_command_pool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device->GetNativeDevice(), m_command_pool, nullptr);
        m_command_pool = VK_NULL_HANDLE;
    }
}

Rndr::AdvancedDeviceQueue::AdvancedDeviceQueue(const AdvancedDevice& device, u32 queue_family_index)
    : m_device(device), m_queue_family_index(queue_family_index)
{
    vkGetDeviceQueue(device.GetNativeDevice(), m_queue_family_index, 0, &m_queue);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = m_queue_family_index;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    const VkResult result = vkCreateCommandPool(m_device->GetNativeDevice(), &pool_info, nullptr, &m_command_pool);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateCommandPool");
    }
}

Rndr::AdvancedDeviceQueue::AdvancedDeviceQueue(AdvancedDeviceQueue&& other) noexcept
    : m_queue(other.m_queue), m_command_pool(other.m_command_pool)
{
    other.m_queue = VK_NULL_HANDLE;
    other.m_command_pool = VK_NULL_HANDLE;
}

Rndr::AdvancedDeviceQueue& Rndr::AdvancedDeviceQueue::operator=(AdvancedDeviceQueue&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    m_queue = other.m_queue;
    m_command_pool = other.m_command_pool;
    other.m_queue = VK_NULL_HANDLE;
    other.m_command_pool = VK_NULL_HANDLE;
    return *this;
}

void Rndr::AdvancedDeviceQueue::Submit(const AdvancedCommandBuffer& command_buffer, const AdvancedFence& fence)
{
    VkCommandBuffer native_command_buffer = command_buffer.GetNativeCommandBuffer();
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &native_command_buffer};
    const VkResult submit_result = vkQueueSubmit(m_queue, 1, &submit_info, fence.GetNativeFence());
    if (submit_result != VK_SUCCESS)
    {
        throw VulkanException(submit_result, "vkQueueSubmit");
    }
}

void Rndr::AdvancedDeviceQueue::Submit(const AdvancedCommandBuffer& command_buffer, const AdvancedSemaphore& wait_semaphore,
                                        PipelineStageBits wait_stage, const AdvancedSemaphore& signal_semaphore,
                                        const AdvancedFence& fence)
{
    VkCommandBuffer native_command_buffer = command_buffer.GetNativeCommandBuffer();
    const VkSemaphore wait_native = wait_semaphore.GetNativeSemaphore();
    const VkSemaphore signal_native = signal_semaphore.GetNativeSemaphore();
    const VkPipelineStageFlags wait_stage_flags = static_cast<VkPipelineStageFlags>(wait_stage);
    const bool has_wait = wait_native != VK_NULL_HANDLE;
    const bool has_signal = signal_native != VK_NULL_HANDLE;
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = has_wait ? 1u : 0u,
        .pWaitSemaphores = has_wait ? &wait_native : nullptr,
        .pWaitDstStageMask = has_wait ? &wait_stage_flags : nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &native_command_buffer,
        .signalSemaphoreCount = has_signal ? 1u : 0u,
        .pSignalSemaphores = has_signal ? &signal_native : nullptr,
    };
    const VkResult submit_result = vkQueueSubmit(m_queue, 1, &submit_info, fence.GetNativeFence());
    if (submit_result != VK_SUCCESS)
    {
        throw VulkanException(submit_result, "vkQueueSubmit");
    }
}
