#pragma once

#include "volk.h"

#include "opal/container/array-view.h"
#include "opal/container/expected.h"
#include "opal/container/ref.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"
#include "rndr/types.hpp"

// Forward declare handle to avoid vma includes in headers.
using VmaAllocation = struct VmaAllocation_T*;

namespace Rndr::Forge
{

struct BufferDesc
{
    size_t size = 0;
    BufferUsageBits usage = BufferUsageBits::None;
    /** Buffer::Read needs HostAccess::Random and is refused without it, since the default is write-only memory. */
    HostAccess host_access = HostAccess::SequentialWrite;
    bool keep_memory_mapped = true;
    bool use_device_address = false;
};

class Buffer
{
public:
    Buffer() = default;
    ~Buffer();

    /**
     * Allocate the buffer and, when initial_data is given, fill it.
     *
     * @param device Device to allocate from. Has to outlive the buffer.
     * @param desc Size, usage, host access and whether to keep the memory mapped.
     * @param initial_data Bytes to write into it. Needs host access; UploadToBuffer is the path for a buffer
     *        the host cannot write.
     * @return The buffer, ErrorCode::InvalidArgument when the desc asks for something this device or this
     *         buffer cannot do - a device address without the feature, initial data that does not fit or
     *         that goes to memory the host cannot write - or whatever the failing allocation maps to.
     */
    [[nodiscard]] static Opal::Expected<Buffer, ErrorCode> Create(const Device& device, const BufferDesc& desc = {},
                                                                  Opal::ArrayView<const u8> initial_data = {});

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
     * Write data into the buffer at the given offset. Non-coherent memory is flushed, so the write is visible
     * to the device once this returns.
     * @return ErrorCode::Success, ErrorCode::OutOfBounds when the write does not fit, ErrorCode::InvalidArgument
     *         when the memory is not one the host can write, or whatever the failing map maps to.
     */
    [[nodiscard]] ErrorCode Update(Opal::ArrayView<const u8> data, size_t offset = 0) const;

    /**
     * Read data out of the buffer at the given offset, filling the whole view. Non-coherent memory is
     * invalidated first, so what the device wrote is what this returns.
     * @return ErrorCode::Success, ErrorCode::OutOfBounds when the read does not fit, ErrorCode::InvalidArgument
     *         when the buffer was not created with HostAccess::Random - reading write-combined memory works
     *         and is slow enough to be a bug - or whatever the failing map maps to.
     */
    [[nodiscard]] ErrorCode Read(Opal::ArrayView<u8> data, size_t offset = 0) const;

private:
    /** Make a host write to the given range visible to the device. Does nothing on coherent memory. */
    [[nodiscard]] ErrorCode Flush(size_t offset, size_t size) const;

    /** Make a device write to the given range visible to the host. Does nothing on coherent memory. */
    [[nodiscard]] ErrorCode Invalidate(size_t offset, size_t size) const;

    BufferDesc m_desc;
    Opal::Ref<const Device> m_device;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceAddress m_device_address = 0;
    void* m_mapped_memory = nullptr;
};

}  // namespace Rndr::Forge