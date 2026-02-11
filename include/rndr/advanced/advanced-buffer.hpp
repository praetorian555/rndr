#pragma once

#include "volk/volk.h"

#include "opal/container/ref.h"
#include "opal/container/array-view.h"

#include "rndr/types.hpp"

// Forward declare handle to avoid vma includes in headers.
using VmaAllocation = struct VmaAllocation_T*;

namespace Rndr
{

struct AdvancedBufferDesc
{
    size_t size;
    VkBufferUsageFlags usage;
    bool keep_memory_mapped = true;
};

class AdvancedBuffer
{
public:
    AdvancedBuffer() = default;
    explicit AdvancedBuffer(const class AdvancedDevice& device, const AdvancedBufferDesc& desc = {}, Opal::ArrayView<u8> initial_data = {});
    ~AdvancedBuffer();

    AdvancedBuffer(const AdvancedBuffer&) = delete;
    AdvancedBuffer& operator=(const AdvancedBuffer&) = delete;
    AdvancedBuffer(AdvancedBuffer&&) noexcept;
    AdvancedBuffer& operator=(AdvancedBuffer&&) noexcept;

    void Destroy();

    [[nodiscard]] VkBuffer GetNativeBuffer() const { return m_buffer; }
    [[nodiscard]] VkDeviceAddress GetNativeDeviceAddress() const { return m_device_address; }
    [[nodiscard]] size_t GetSize() const { return m_desc.size; }

    void Update(Opal::ArrayView<const u8> data, size_t offset) const;

private:
    AdvancedBufferDesc m_desc;
    Opal::Ref<const class AdvancedDevice> m_device;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation;
    VkDeviceAddress m_device_address;
    void* m_mapped_memory = nullptr;
};

}  // namespace Rndr