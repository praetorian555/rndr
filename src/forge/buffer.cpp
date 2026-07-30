#include "rndr/forge/buffer.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "vma/vk_mem_alloc.h"

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-exception.hpp"

Rndr::Forge::Buffer::Buffer(const Device& device, const BufferDesc& desc, Opal::ArrayView<u8> initial_data)
    : m_device(device), m_desc(desc)
{
    VkBufferCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = desc.size, .usage = desc.usage};
    if (desc.use_device_address)
    {
        create_info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
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
        throw VulkanException(result, "vmaCreateBuffer");
    }
    if (!initial_data.IsEmpty())
    {
        if (initial_data.GetSize() > m_desc.size)
        {
            throw Opal::Exception("Initial data does not fit into the buffer.");
        }
        void* gpu_data = nullptr;
        result = vmaMapMemory(m_device->GetGPUAllocator(), m_allocation, &gpu_data);
        if (result != VK_SUCCESS)
        {
            throw VulkanException(result, "vmaMapMemory");
        }
        memcpy(gpu_data, initial_data.GetData(), initial_data.GetSize());
        vmaUnmapMemory(m_device->GetGPUAllocator(), m_allocation);
        Flush(0, initial_data.GetSize());
    }
    if (m_desc.keep_memory_mapped)
    {
        result = vmaMapMemory(m_device->GetGPUAllocator(), m_allocation, &m_mapped_memory);
        if (result != VK_SUCCESS)
        {
            throw VulkanException(result, "vmaMapMemory");
        }
    }
    const VkBufferDeviceAddressInfo buffer_device_address_info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = m_buffer};
    if (m_desc.use_device_address)
    {
        m_device_address = vkGetBufferDeviceAddress(m_device->GetNativeDevice(), &buffer_device_address_info);
        if (m_device_address == 0)
        {
            throw Opal::Exception("Failed to get buffer device address.");
        }
    }
}

Rndr::Forge::Buffer::~Buffer()
{
    Destroy();
}

Rndr::Forge::Buffer::Buffer(Buffer&& other) noexcept
    : m_desc(other.m_desc),
      m_device(std::move(other.m_device)),
      m_buffer(other.m_buffer),
      m_allocation(other.m_allocation),
      m_device_address(other.m_device_address),
      m_mapped_memory(other.m_mapped_memory)
{
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_device_address = 0;
    other.m_mapped_memory = nullptr;
}

Rndr::Forge::Buffer& Rndr::Forge::Buffer::operator=(Buffer&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_device = std::move(other.m_device);
        m_buffer = other.m_buffer;
        m_allocation = other.m_allocation;
        m_device_address = other.m_device_address;
        m_mapped_memory = other.m_mapped_memory;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
        other.m_device = nullptr;
        other.m_device_address = 0;
        other.m_mapped_memory = nullptr;
    }
    return *this;
}

void Rndr::Forge::Buffer::Destroy()
{
    if (m_buffer != VK_NULL_HANDLE)
    {
        // Keyed off the pointer rather than the desc, so that a buffer that was moved from does not try to unmap
        // memory it no longer owns.
        if (m_mapped_memory != nullptr)
        {
            vmaUnmapMemory(m_device->GetGPUAllocator(), m_allocation);
        }
        vmaDestroyBuffer(m_device->GetGPUAllocator(), m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
    m_mapped_memory = nullptr;
    m_device = nullptr;
    m_device_address = 0;
}

void Rndr::Forge::Buffer::Flush(size_t offset, size_t size) const
{
    // VMA compares the memory type against HOST_COHERENT and skips the flush when it is not needed, and it rounds
    // the range out to nonCoherentAtomSize itself.
    const VkResult result = vmaFlushAllocation(m_device->GetGPUAllocator(), m_allocation, offset, size);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vmaFlushAllocation");
    }
}

void Rndr::Forge::Buffer::Update(Opal::ArrayView<const u8> data, size_t offset) const
{
    if (data.IsEmpty())
    {
        return;
    }
    // Written this way around so that a huge offset cannot overflow the sum and pass the check.
    if (offset > m_desc.size || data.GetSize() > m_desc.size - offset)
    {
        throw Opal::Exception("Update does not fit into the buffer.");
    }
    if (m_mapped_memory != nullptr)
    {
        memcpy(static_cast<u8*>(m_mapped_memory) + offset, data.GetData(), data.GetSize());
        Flush(offset, data.GetSize());
        return;
    }
    void* gpu_data = nullptr;
    const VkResult result = vmaMapMemory(m_device->GetGPUAllocator(), m_allocation, &gpu_data);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vmaMapMemory");
    }
    memcpy(static_cast<u8*>(gpu_data) + offset, data.GetData(), data.GetSize());
    vmaUnmapMemory(m_device->GetGPUAllocator(), m_allocation);
    Flush(offset, data.GetSize());
}