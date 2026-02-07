#include "rndr/advanced/device.hpp"

#include "rndr/advanced/command-buffer.hpp"
#include "rndr/advanced/graphics-context.hpp"
#include "rndr/advanced/swap-chain.hpp"
#include "rndr/advanced/synchronization.hpp"

Opal::DynamicArray<Rndr::u32> Rndr::AdvancedQueueFamilyIndices::GetValidQueueFamilies() const
{
    Opal::DynamicArray<u32> valid_queue_families;
    if (graphics_family != k_invalid_index)
    {
        valid_queue_families.PushBack(graphics_family);
    }
    if (present_family != k_invalid_index && present_family != graphics_family)
    {
        valid_queue_families.PushBack(present_family);
    }
    if (transfer_family != k_invalid_index)
    {
        valid_queue_families.PushBack(transfer_family);
    }
    if (compute_family != k_invalid_index)
    {
        valid_queue_families.PushBack(compute_family);
    }
    return valid_queue_families;
}

Rndr::AdvancedDevice::AdvancedDevice(AdvancedPhysicalDevice physical_device, const AdvancedGraphicsContext& graphics_context,
                                     const AdvancedDeviceDesc& desc)
{
    if (!physical_device.IsValid())
    {
        throw Opal::Exception("Physical device is invalid!");
    }

    constexpr f32 k_queue_priority = 1.0f;
    AdvancedQueueFamilyIndices queue_family_indices;
    Opal::DynamicArray<VkDeviceQueueCreateInfo> queue_create_infos;
    auto queue_family_index = physical_device.GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    if (((desc.queue_flags & VK_QUEUE_GRAPHICS_BIT) != 0u) && queue_family_index.HasValue())
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_index.GetValue();
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &k_queue_priority;
        queue_create_infos.PushBack(queue_create_info);
        queue_family_indices.graphics_family = queue_family_index.GetValue();
    }

    if (desc.surface.IsValid())
    {
        auto details = desc.surface->GetSwapChainSupportDetails(physical_device);
        auto present_queue_family_index = physical_device.GetPresentQueueFamilyIndex(desc.surface);
        if (!present_queue_family_index.HasValue())
        {
            throw Opal::Exception("Present queue family index is invalid!");
        }
        queue_family_indices.present_family = present_queue_family_index.GetValue();
        if (present_queue_family_index.GetValue() != queue_family_index.GetValue())
        {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = present_queue_family_index.GetValue();
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &k_queue_priority;
            queue_create_infos.PushBack(queue_create_info);
        }
    }

    Opal::DynamicArray device_extensions(desc.extensions);
    if (desc.surface.IsValid())
    {
        device_extensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    for (const char* extension_name : device_extensions)
    {
        if (!physical_device.IsExtensionSupported(extension_name))
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

    VkResult result = vkCreateDevice(physical_device.GetNativePhysicalDevice(), &create_info, nullptr, &m_device);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create device!");
    }

    auto queue_family_indices_array = queue_family_indices.GetValidQueueFamilies();
    for (u32 index : queue_family_indices_array)
    {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = queue_family_indices.graphics_family;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkCommandPool command_pool{};
        result = vkCreateCommandPool(m_device, &pool_info, nullptr, &command_pool);
        if (result != VK_SUCCESS)
        {
            throw Opal::Exception("Failed to create command pool!");
        }
        m_queue_family_index_to_command_pool.Insert(index, command_pool);
    }

    m_physical_device = Opal::Move(physical_device);
    m_desc = desc;
    m_queue_family_indices = queue_family_indices;

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
    if (vmaCreateAllocator(&vma_alloc_create_info, &m_gpu_allocator) != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create allocator!");
    }
}

Rndr::AdvancedDevice::~AdvancedDevice()
{
    Destroy();
}

Rndr::AdvancedDevice::AdvancedDevice(AdvancedDevice&& other) noexcept
    : m_device(other.m_device),
      m_queue_family_index_to_command_pool(Opal::Move(other.m_queue_family_index_to_command_pool)),
      m_physical_device(Opal::Move(other.m_physical_device)),
      m_desc(Opal::Move(other.m_desc)),
      m_queue_family_indices(other.m_queue_family_indices)
{
    other.m_device = VK_NULL_HANDLE;
    other.m_queue_family_index_to_command_pool.Clear();
    other.m_physical_device = {};
    other.m_desc = {};
    other.m_queue_family_indices = {};
}

Rndr::AdvancedDevice& Rndr::AdvancedDevice::operator=(AdvancedDevice&& other) noexcept
{
    Destroy();

    m_device = other.m_device;
    m_queue_family_index_to_command_pool = Opal::Move(other.m_queue_family_index_to_command_pool);
    m_physical_device = Opal::Move(other.m_physical_device);
    m_desc = Opal::Move(other.m_desc);
    m_queue_family_indices = other.m_queue_family_indices;

    other.m_device = VK_NULL_HANDLE;
    other.m_queue_family_index_to_command_pool.Clear();
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
    for (auto p : m_queue_family_index_to_command_pool)
    {
        vkDestroyCommandPool(m_device, p.value, nullptr);
    }
    m_queue_family_index_to_command_pool.Clear();
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    m_physical_device = {};
    m_desc = {};
}

VkCommandBuffer Rndr::AdvancedDevice::CreateCommandBuffer(u32 queue_family_index) const
{
    return CreateCommandBuffers(queue_family_index, 1)[0];
}

Opal::DynamicArray<VkCommandBuffer> Rndr::AdvancedDevice::CreateCommandBuffers(u32 queue_family_index, u32 count) const
{
    if (count == 0)
    {
        throw Opal::Exception("Count must be greater than zero!");
    }

    Opal::DynamicArray<VkCommandBuffer> command_buffers;
    const auto it = m_queue_family_index_to_command_pool.Find(queue_family_index);
    if (it == m_queue_family_index_to_command_pool.end())
    {
        throw Opal::Exception("Queue family index not supported!");
    }

    command_buffers.Resize(count);
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = it.GetValue();
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = count;
    const VkResult result = vkAllocateCommandBuffers(m_device, &alloc_info, command_buffers.GetData());
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to allocate command buffers!");
    }
    return command_buffers;
}

bool Rndr::AdvancedDevice::DestroyCommandBuffer(VkCommandBuffer command_buffer, u32 queue_family_index) const
{
    const auto it = m_queue_family_index_to_command_pool.Find(queue_family_index);
    if (it == m_queue_family_index_to_command_pool.end())
    {
        throw Opal::Exception("Queue family index not supported!");
    }
    vkFreeCommandBuffers(m_device, it.GetValue(), 1, &command_buffer);
    return true;
}

bool Rndr::AdvancedDevice::DestroyCommandBuffers(const Opal::DynamicArray<VkCommandBuffer>& command_buffers, u32 queue_family_index) const
{
    const auto it = m_queue_family_index_to_command_pool.Find(queue_family_index);
    if (it == m_queue_family_index_to_command_pool.end())
    {
        throw Opal::Exception("Queue family index not supported!");
    }
    vkFreeCommandBuffers(m_device, it.GetValue(), static_cast<u32>(command_buffers.GetSize()), command_buffers.GetData());
    return true;
}

VkDescriptorPool Rndr::AdvancedDevice::CreateDescriptorPool(const AdvancedDescriptorPoolDesc& desc) const
{
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<u32>(desc.pool_sizes.GetSize());
    pool_info.pPoolSizes = desc.pool_sizes.GetData();
    pool_info.maxSets = desc.max_sets;
    pool_info.flags = desc.flags;

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    const VkResult result = vkCreateDescriptorPool(m_device, &pool_info, nullptr, &descriptor_pool);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create descriptor pool!");
    }
    return descriptor_pool;
}

bool Rndr::AdvancedDevice::DestroyDescriptorPool(VkDescriptorPool descriptor_pool) const
{
    vkDestroyDescriptorPool(m_device, descriptor_pool, nullptr);
    return true;
}

VkDescriptorSetLayout Rndr::AdvancedDevice::CreateDescriptorSetLayout(const AdvancedDescriptorSetLayoutDesc& desc) const
{
    Opal::DynamicArray<VkDescriptorSetLayoutBinding> bindings(desc.bindings.GetSize());
    for (i32 i = 0; i < bindings.GetSize(); i++)
    {
        VkDescriptorSetLayoutBinding& binding = bindings[i];
        binding.binding = desc.bindings[i].binding;
        binding.descriptorType = desc.bindings[i].descriptor_type;
        binding.descriptorCount = desc.bindings[i].descriptor_count;
        binding.stageFlags = desc.bindings[i].stage_flags;
        binding.pImmutableSamplers = desc.bindings[i].sampler;
    }

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<u32>(bindings.GetSize());
    layout_info.pBindings = bindings.GetData();

    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    const VkResult result = vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &descriptor_set_layout);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create descriptor set layout!");
    }
    return descriptor_set_layout;
}

bool Rndr::AdvancedDevice::DestroyDescriptorSetLayout(VkDescriptorSetLayout descriptor_set_layout) const
{
    vkDestroyDescriptorSetLayout(m_device, descriptor_set_layout, nullptr);
    return true;
}

Opal::DynamicArray<VkDescriptorSet> Rndr::AdvancedDevice::AllocateDescriptorSets(const VkDescriptorPool& descriptor_pool, u32 count,
                                                                                 const VkDescriptorSetLayout& layout) const
{
    Opal::DynamicArray<VkDescriptorSetLayout> layouts(count, layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = count;
    alloc_info.pSetLayouts = layouts.GetData();

    Opal::DynamicArray<VkDescriptorSet> descriptor_sets(count);
    const VkResult result = vkAllocateDescriptorSets(m_device, &alloc_info, descriptor_sets.GetData());
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to allocate descriptor sets!");
    }
    return descriptor_sets;
}

void Rndr::AdvancedDevice::UpdateDescriptorSets(const Opal::DynamicArray<AdvancedUpdateDescriptorSet>& updates) const
{
    for (i32 i = 0; i < updates.GetSize(); i++)
    {
        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = updates[i].descriptor_set;
        descriptor_write.dstBinding = updates[i].binding;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = updates[i].descriptor_type;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &updates[i].buffer_info;
        vkUpdateDescriptorSets(m_device, 1, &descriptor_write, 0, nullptr);
    }
}

Rndr::AdvancedDeviceQueue::AdvancedDeviceQueue(const AdvancedDevice& device, AdvancedDeviceQueueFamilyFlags queue_family_flags)
    : m_device(device)
{
    auto queue_index = device.GetPhysicalDevice().GetQueueFamilyIndex(static_cast<VkQueueFlags>(queue_family_flags));
    if (!queue_index.HasValue())
    {
        throw Opal::Exception("Queue family index does not exist!");
    }
    m_queue_family_index = queue_index.GetValue();
    vkGetDeviceQueue(device.GetNativeDevice(), m_queue_family_index, 0, &m_queue);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = m_queue_family_index;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    const VkResult result = vkCreateCommandPool(m_device->GetNativeDevice(), &pool_info, nullptr, &m_command_pool);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create command pool!");
    }
}

Rndr::AdvancedDeviceQueue::AdvancedDeviceQueue(AdvancedDeviceQueue&& other) noexcept : m_queue(other.m_queue)
{
    other.m_queue = VK_NULL_HANDLE;
}

Rndr::AdvancedDeviceQueue& Rndr::AdvancedDeviceQueue::operator=(AdvancedDeviceQueue&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    m_queue = other.m_queue;
    other.m_queue = VK_NULL_HANDLE;
    return *this;
}

Opal::DynamicArray<VkCommandBuffer> Rndr::AdvancedDeviceQueue::CreateCommandBuffers(u32 count) const
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = count;

    Opal::DynamicArray<VkCommandBuffer> command_buffers(count);
    const VkResult result = vkAllocateCommandBuffers(m_device->GetNativeDevice(), &alloc_info, command_buffers.GetData());
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to allocate command buffer!");
    }
    return command_buffers;
}

void Rndr::AdvancedDeviceQueue::DestroyCommandBuffer(VkCommandBuffer command_buffer) const
{
    vkFreeCommandBuffers(m_device->GetNativeDevice(), m_command_pool, 1, &command_buffer);
}

void Rndr::AdvancedDeviceQueue::DestroyCommandBuffers(Opal::ArrayView<VkCommandBuffer> command_buffers) const
{
    vkFreeCommandBuffers(m_device->GetNativeDevice(), m_command_pool, static_cast<u32>(command_buffers.GetSize()),
                         command_buffers.GetData());
}

void Rndr::AdvancedDeviceQueue::Submit(const AdvancedCommandBuffer& command_buffer, const AdvancedFence& fence)
{
    VkCommandBuffer native_command_buffer = command_buffer.GetNativeCommandBuffer();
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &native_command_buffer};
    if (vkQueueSubmit(m_queue, 1, &submit_info, fence.GetNativeFence()) != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to submit command buffer!");
    }
}
