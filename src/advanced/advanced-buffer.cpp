#include "rndr/advanced/advanced-buffer.hpp"

#include "rndr/advanced/device.hpp"

Rndr::AdvancedBuffer::AdvancedBuffer(const class AdvancedDevice& device, const AdvancedBufferDesc& desc, Opal::ArrayView<u8> initial_data)
    : m_device(device), m_desc(desc)
{
    const VkBufferCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = desc.size, .usage = desc.usage};
    // First flag means that buffer can be mapped on the CPU side and that writing will be done by either memcpy or sequential
    // iteration over the elements, there will be no random access.
    // Second flag means that we want to choose memory type that will improve performance but might not be host visible.
    // This combo allows us on newer hardware to have high performance GPU memory for the buffer but still be able to access it on CPU.
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
        vmaDestroyBuffer(m_device->GetGPUAllocator(), m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
    m_device = nullptr;
}

void Rndr::AdvancedBuffer::Update(Opal::ArrayView<u8> data, size_t) const
{
    if (data.IsEmpty())
    {
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