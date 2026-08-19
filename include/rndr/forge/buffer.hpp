#pragma once

#include "volk/volk.h"

#include "opal/container/ref.h"
#include "opal/container/array-view.h"

#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"

// Forward declare handle to avoid vma includes in headers.
using VmaAllocation = struct VmaAllocation_T*;

namespace Rndr::Forge
{

struct BufferDesc
{
    size_t size = 0;
    BufferUsageBits usage = BufferUsageBits::None;
    /** Buffer::Read needs HostAccess::Random and throws without it, since the default is write-only memory. */
    HostAccess host_access = HostAccess::SequentialWrite;
    bool keep_memory_mapped = true;
    bool use_device_address = false;
};

class Buffer
{
public:
    Buffer() = default;
    explicit Buffer(const Device& device, const BufferDesc& desc = {}, Opal::ArrayView<const u8> initial_data = {});
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_buffer != VK_NULL_HANDLE; }
    [[nodiscard]] VkBuffer GetNativeBuffer() const { return m_buffer; }
    [[nodiscard]] VkDeviceAddress GetNativeDeviceAddress() const { return m_device_address; }
    [[nodiscard]] size_t GetSize() const { return m_desc.size; }
    [[nodiscard]] const BufferDesc& GetDesc() const { return m_desc; }

    /**
     * Write data into the buffer at the given offset. Throws when the write does not fit. Non-coherent memory is
     * flushed, so the write is visible to the device once this returns.
     */
    void Update(Opal::ArrayView<const u8> data, size_t offset = 0) const;

    /**
     * Read data out of the buffer at the given offset, filling the whole view. Throws when the read does not
     * fit, and when the buffer was not created with HostAccess::Random - reading write-combined memory works
     * and is slow enough to be a bug. Non-coherent memory is invalidated first, so what the device wrote is
     * what this returns.
     */
    void Read(Opal::ArrayView<u8> data, size_t offset = 0) const;

private:
    /** Make a host write to the given range visible to the device. Does nothing on coherent memory. */
    void Flush(size_t offset, size_t size) const;

    /** Make a device write to the given range visible to the host. Does nothing on coherent memory. */
    void Invalidate(size_t offset, size_t size) const;

    BufferDesc m_desc;
    Opal::Ref<const Device> m_device;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceAddress m_device_address = 0;
    void* m_mapped_memory = nullptr;
};

}  // namespace Rndr::Forge