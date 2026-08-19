#include "rndr/forge/buffer.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "vma/vk_mem_alloc.h"

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-exception.hpp"

Rndr::Forge::Buffer::Buffer(const Device& device, const BufferDesc& desc, Opal::ArrayView<const u8> initial_data)
    : m_device(device), m_desc(desc)
{
    // Checked before anything is created: the allocation below asks for device address memory, which is
    // already a validation error on a device without the feature.
    if (desc.use_device_address && !m_device->GetFeatures().buffer_device_address)
    {
        throw Opal::Exception("A buffer with a device address needs the device created with DeviceFeatures::buffer_device_address.");
    }
    // The values of BufferUsageBits mirror VkBufferUsageFlagBits, so the mask translates as a cast.
    VkBufferCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                   .size = desc.size,
                                   .usage = static_cast<VkBufferUsageFlags>(desc.usage)};
    if (desc.use_device_address)
    {
        create_info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    // Asking for host access is a promise that the memory can be mapped, so the allocation is not allowed to
    // fall back to memory the host cannot see. HostAccess::None is how a caller asks for that fallback, and
    // then the staging helpers of transfer.hpp are what fill the buffer.
    VmaAllocationCreateFlags allocation_flags = 0;
    switch (desc.host_access)
    {
        case HostAccess::SequentialWrite:
            allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case HostAccess::Random:
            allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case HostAccess::None:
            break;
        default:
            throw Opal::Exception("Unknown host access!");
    }
    const VmaAllocationCreateInfo allocation_create_info{.flags = allocation_flags, .usage = VMA_MEMORY_USAGE_AUTO};
    VkResult result =
        vmaCreateBuffer(m_device->GetGPUAllocator(), &create_info, &allocation_create_info, &m_buffer, &m_allocation, nullptr);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vmaCreateBuffer");
    }
    // Everything past this point can throw, and the destructor does not run for an object whose constructor
    // did, so the allocation above would be leaked without this.
    try
    {
        if (!initial_data.IsEmpty())
        {
            if (m_desc.host_access == HostAccess::None)
            {
                throw Opal::Exception("A buffer with HostAccess::None cannot take initial data - use UploadToBuffer.");
            }
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
        if (m_desc.keep_memory_mapped && m_desc.host_access != HostAccess::None)
        {
            result = vmaMapMemory(m_device->GetGPUAllocator(), m_allocation, &m_mapped_memory);
            if (result != VK_SUCCESS)
            {
                throw VulkanException(result, "vmaMapMemory");
            }
        }
        if (m_desc.use_device_address)
        {
            const VkBufferDeviceAddressInfo buffer_device_address_info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                                       .buffer = m_buffer};
            m_device_address = vkGetBufferDeviceAddress(m_device->GetNativeDevice(), &buffer_device_address_info);
            if (m_device_address == 0)
            {
                throw Opal::Exception("Failed to get buffer device address.");
            }
        }
    }
    catch (...)
    {
        Destroy();
        throw;
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

void Rndr::Forge::Buffer::Invalidate(size_t offset, size_t size) const
{
    // The counterpart of Flush. VMA skips it on coherent memory and rounds the range out to nonCoherentAtomSize itself.
    const VkResult result = vmaInvalidateAllocation(m_device->GetGPUAllocator(), m_allocation, offset, size);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vmaInvalidateAllocation");
    }
}

void Rndr::Forge::Buffer::Update(Opal::ArrayView<const u8> data, size_t offset) const
{
    if (data.IsEmpty())
    {
        return;
    }
    if (m_desc.host_access == HostAccess::None)
    {
        throw Opal::Exception("Writing a buffer with HostAccess::None is not possible - use UploadToBuffer.");
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

void Rndr::Forge::Buffer::Read(Opal::ArrayView<u8> data, size_t offset) const
{
    if (data.IsEmpty())
    {
        return;
    }
    if (m_desc.host_access != HostAccess::Random)
    {
        throw Opal::Exception("Reading a buffer needs HostAccess::Random - the other kinds are write-combined or not mapped at all.");
    }
    // Written this way around so that a huge offset cannot overflow the sum and pass the check.
    if (offset > m_desc.size || data.GetSize() > m_desc.size - offset)
    {
        throw Opal::Exception("Read does not fit into the buffer.");
    }
    Invalidate(offset, data.GetSize());
    if (m_mapped_memory != nullptr)
    {
        memcpy(data.GetData(), static_cast<const u8*>(m_mapped_memory) + offset, data.GetSize());
        return;
    }
    void* gpu_data = nullptr;
    const VkResult result = vmaMapMemory(m_device->GetGPUAllocator(), m_allocation, &gpu_data);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vmaMapMemory");
    }
    memcpy(data.GetData(), static_cast<const u8*>(gpu_data) + offset, data.GetSize());
    vmaUnmapMemory(m_device->GetGPUAllocator(), m_allocation);
}