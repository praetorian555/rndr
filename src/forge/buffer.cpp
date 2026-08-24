#include "rndr/forge/buffer.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "vma/vk_mem_alloc.h"

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/log.hpp"

Opal::Expected<Rndr::Forge::Buffer, Rndr::ErrorCode> Rndr::Forge::Buffer::Create(const Device& device, const BufferDesc& desc,
                                                                                 Opal::ArrayView<const u8> initial_data)
{
    using Result = Opal::Expected<Buffer, ErrorCode>;

    // Checked before anything is created: the allocation below asks for device address memory, which is
    // already a validation error on a device without the feature.
    if (desc.use_device_address && !device.GetFeatures().buffer_device_address)
    {
        RNDR_LOG_ERROR("Forge: a buffer with a device address needs the device created with DeviceFeatures::buffer_device_address");
        return Result(ErrorCode::InvalidArgument);
    }

    // Everything below is built into this, so a way out of here releases what got that far through the
    // destructor rather than by hand.
    Buffer buffer;
    buffer.m_device = device;
    buffer.m_desc = desc;
    // The values of BufferUsageBits mirror VkBufferUsageFlagBits, so the mask translates as a cast.
    VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = desc.size, .usage = static_cast<VkBufferUsageFlags>(desc.usage)};
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
            RNDR_LOG_ERROR("Forge: BufferDesc::host_access is not one of the three: {}", static_cast<u32>(desc.host_access));
            return Result(ErrorCode::InvalidArgument);
    }
    const VmaAllocationCreateInfo allocation_create_info{.flags = allocation_flags, .usage = VMA_MEMORY_USAGE_AUTO};
    RNDR_FORGE_VK_CHECK_EXPECTED(
        vmaCreateBuffer(device.GetGPUAllocator(), &create_info, &allocation_create_info, &buffer.m_buffer, &buffer.m_allocation, nullptr),
        "vmaCreateBuffer", Result);

    if (!initial_data.IsEmpty())
    {
        if (desc.host_access == HostAccess::None)
        {
            RNDR_LOG_ERROR("Forge: a buffer with HostAccess::None cannot take initial data - use UploadToBuffer");
            return Result(ErrorCode::InvalidArgument);
        }
        if (initial_data.GetSize() > desc.size)
        {
            RNDR_LOG_ERROR("Forge: initial data of {} bytes does not fit into a buffer of {}", initial_data.GetSize(), desc.size);
            return Result(ErrorCode::OutOfBounds);
        }
        void* gpu_data = nullptr;
        RNDR_FORGE_VK_CHECK_EXPECTED(vmaMapMemory(device.GetGPUAllocator(), buffer.m_allocation, &gpu_data), "vmaMapMemory", Result);
        memcpy(gpu_data, initial_data.GetData(), initial_data.GetSize());
        vmaUnmapMemory(device.GetGPUAllocator(), buffer.m_allocation);
        const ErrorCode flush_status = buffer.Flush(0, initial_data.GetSize());
        if (flush_status != ErrorCode::Success)
        {
            return Result(flush_status);
        }
    }
    if (desc.keep_memory_mapped && desc.host_access != HostAccess::None)
    {
        RNDR_FORGE_VK_CHECK_EXPECTED(vmaMapMemory(device.GetGPUAllocator(), buffer.m_allocation, &buffer.m_mapped_memory), "vmaMapMemory",
                                     Result);
    }
    if (desc.use_device_address)
    {
        const VkBufferDeviceAddressInfo buffer_device_address_info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                                   .buffer = buffer.m_buffer};
        buffer.m_device_address = vkGetBufferDeviceAddress(device.GetNativeDevice(), &buffer_device_address_info);
        if (buffer.m_device_address == 0)
        {
            RNDR_LOG_ERROR("Forge: the device gave no address for a buffer that asked for one");
            return Result(ErrorCode::GraphicsAPIError);
        }
    }
    return Result(std::move(buffer));
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

Rndr::ErrorCode Rndr::Forge::Buffer::Flush(size_t offset, size_t size) const
{
    // VMA compares the memory type against HOST_COHERENT and skips the flush when it is not needed, and it rounds
    // the range out to nonCoherentAtomSize itself.
    RNDR_FORGE_VK_CHECK(vmaFlushAllocation(m_device->GetGPUAllocator(), m_allocation, offset, size), "vmaFlushAllocation");
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::Buffer::Invalidate(size_t offset, size_t size) const
{
    // The counterpart of Flush. VMA skips it on coherent memory and rounds the range out to nonCoherentAtomSize itself.
    RNDR_FORGE_VK_CHECK(vmaInvalidateAllocation(m_device->GetGPUAllocator(), m_allocation, offset, size), "vmaInvalidateAllocation");
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::Buffer::Update(Opal::ArrayView<const u8> data, size_t offset) const
{
    if (data.IsEmpty())
    {
        return ErrorCode::Success;
    }
    if (m_desc.host_access == HostAccess::None)
    {
        RNDR_LOG_ERROR("Forge: writing a buffer with HostAccess::None is not possible - use UploadToBuffer");
        return ErrorCode::InvalidArgument;
    }
    // Written this way around so that a huge offset cannot overflow the sum and pass the check.
    if (offset > m_desc.size || data.GetSize() > m_desc.size - offset)
    {
        RNDR_LOG_ERROR("Forge: an update of {} bytes at offset {} does not fit into a buffer of {}", data.GetSize(), offset, m_desc.size);
        return ErrorCode::OutOfBounds;
    }
    if (m_mapped_memory != nullptr)
    {
        memcpy(static_cast<u8*>(m_mapped_memory) + offset, data.GetData(), data.GetSize());
        return Flush(offset, data.GetSize());
    }
    void* gpu_data = nullptr;
    RNDR_FORGE_VK_CHECK(vmaMapMemory(m_device->GetGPUAllocator(), m_allocation, &gpu_data), "vmaMapMemory");
    memcpy(static_cast<u8*>(gpu_data) + offset, data.GetData(), data.GetSize());
    vmaUnmapMemory(m_device->GetGPUAllocator(), m_allocation);
    return Flush(offset, data.GetSize());
}

Rndr::ErrorCode Rndr::Forge::Buffer::Read(Opal::ArrayView<u8> data, size_t offset) const
{
    if (data.IsEmpty())
    {
        return ErrorCode::Success;
    }
    if (m_desc.host_access != HostAccess::Random)
    {
        RNDR_LOG_ERROR("Forge: reading a buffer needs HostAccess::Random - the other kinds are write-combined or not mapped at all");
        return ErrorCode::InvalidArgument;
    }
    // Written this way around so that a huge offset cannot overflow the sum and pass the check.
    if (offset > m_desc.size || data.GetSize() > m_desc.size - offset)
    {
        RNDR_LOG_ERROR("Forge: a read of {} bytes at offset {} does not fit into a buffer of {}", data.GetSize(), offset, m_desc.size);
        return ErrorCode::OutOfBounds;
    }
    const ErrorCode invalidate_status = Invalidate(offset, data.GetSize());
    if (invalidate_status != ErrorCode::Success)
    {
        return invalidate_status;
    }
    if (m_mapped_memory != nullptr)
    {
        memcpy(data.GetData(), static_cast<const u8*>(m_mapped_memory) + offset, data.GetSize());
        return ErrorCode::Success;
    }
    void* gpu_data = nullptr;
    RNDR_FORGE_VK_CHECK(vmaMapMemory(m_device->GetGPUAllocator(), m_allocation, &gpu_data), "vmaMapMemory");
    memcpy(data.GetData(), static_cast<const u8*>(gpu_data) + offset, data.GetSize());
    vmaUnmapMemory(m_device->GetGPUAllocator(), m_allocation);
    return ErrorCode::Success;
}