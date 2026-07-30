#pragma once

#include "volk/volk.h"

#include "opal/container/ref.h"
#include "opal/container/array-view.h"

#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"

// Forward declare handle to avoid vma includes in headers.
using VmaAllocation = struct VmaAllocation_T*;

namespace Rndr::Forge
{

struct BufferDesc
{
    size_t size;
    VkBufferUsageFlags usage;
    bool keep_memory_mapped = true;
    bool use_device_address = false;
};

class Buffer
{
public:
    Buffer() = default;
    explicit Buffer(const Device& device, const BufferDesc& desc = {}, Opal::ArrayView<u8> initial_data = {});
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;

    void Destroy();

    [[nodiscard]] VkBuffer GetNativeBuffer() const { return m_buffer; }
    [[nodiscard]] VkDeviceAddress GetNativeDeviceAddress() const { return m_device_address; }
    [[nodiscard]] size_t GetSize() const { return m_desc.size; }

    /**
     * Write data into the buffer at the given offset. Throws when the write does not fit. Non-coherent memory is
     * flushed, so the write is visible to the device once this returns.
     */
    void Update(Opal::ArrayView<const u8> data, size_t offset = 0) const;

private:
    /** Make a host write to the given range visible to the device. Does nothing on coherent memory. */
    void Flush(size_t offset, size_t size) const;

    BufferDesc m_desc;
    Opal::Ref<const Device> m_device;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceAddress m_device_address = 0;
    void* m_mapped_memory = nullptr;
};

}  // namespace Rndr::Forge