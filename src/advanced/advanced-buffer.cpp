#include "rndr/advanced/advanced-buffer.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "vma/vk_mem_alloc.h"

#include "rndr/advanced/device.hpp"

Rndr::AdvancedBuffer::AdvancedBuffer(const class AdvancedDevice& device, const AdvancedBufferDesc& desc, Opal::ArrayView<u8> initial_data)
    : m_device(device), m_desc(desc)
{
    const VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = desc.size, .usage = desc.usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
    // First two flags ensure that we get local memory that is host visible if possible, otherwise it fallbacks to invisible local memory
    // for fast GPU access.
    const VmaAllocationCreateInfo allocation_create_info{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                                  VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                                                                  VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                                         .usage = VMA_MEMORY_USAGE_AUTO};
    VkResult result =
        vmaCreateBuffer(m_device->GetGPUAllocator(), &create_info, &allocation_create_info, &m_buffer, &m_allocation, nullptr);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create buffer");
    }
    if (!initial_data.IsEmpty())
    {
        void* gpu_data = nullptr;
        result = vmaMapMemory(m_device->GetGPUAllocator(), m_allocation, &gpu_data);
        if (result != VK_SUCCESS)
        {
            throw Opal::Exception("Failed to map memory");
        }
        memcpy(gpu_data, initial_data.GetData(), initial_data.GetSize());
        vmaUnmapMemory(m_device->GetGPUAllocator(), m_allocation);
    }
    if (m_desc.keep_memory_mapped)
    {
        result = vmaMapMemory(m_device->GetGPUAllocator(), m_allocation, &m_mapped_memory);
        if (result != VK_SUCCESS)
        {
            throw Opal::Exception("Failed to map memory");
        }
    }
    const VkBufferDeviceAddressInfo buffer_device_address_info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = m_buffer};
    m_device_address = vkGetBufferDeviceAddress(m_device->GetNativeDevice(), &buffer_device_address_info);
}

Rndr::AdvancedBuffer::~AdvancedBuffer()
{
    Destroy();
}

Rndr::AdvancedBuffer::AdvancedBuffer(AdvancedBuffer&& other) noexcept
    : m_desc(other.m_desc), m_device(std::move(other.m_device)), m_buffer(other.m_buffer), m_allocation(other.m_allocation)
{
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::AdvancedBuffer& Rndr::AdvancedBuffer::operator=(AdvancedBuffer&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_device = std::move(other.m_device);
        m_buffer = other.m_buffer;
        m_allocation = other.m_allocation;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::AdvancedBuffer::Destroy()
{
    if (m_buffer != VK_NULL_HANDLE)
    {
        if (m_desc.keep_memory_mapped)
        {
            vmaUnmapMemory(m_device->GetGPUAllocator(), m_allocation);
        }
        vmaDestroyBuffer(m_device->GetGPUAllocator(), m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
    m_device = nullptr;
}

void Rndr::AdvancedBuffer::Update(Opal::ArrayView<const u8> data, size_t) const
{
    if (data.IsEmpty())
    {
        return;
    }
    if (m_mapped_memory != nullptr)
    {
        memcpy(m_mapped_memory, data.GetData(), data.GetSize());
        return;
    }
    void* gpu_data = nullptr;
    const VkResult result = vmaMapMemory(m_device->GetGPUAllocator(), m_allocation, &gpu_data);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to map memory");
    }
    memcpy(gpu_data, data.GetData(), data.GetSize());
    vmaUnmapMemory(m_device->GetGPUAllocator(), m_allocation);
}