#pragma once

#include "volk.h"

#include "opal/container/array-view.h"
#include "opal/container/expected.h"
#include "opal/container/optional.h"
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
    /**
     * Refuse to fall back from device local memory. The allocator already prefers it for a buffer the device
     * uses, and takes system memory when there is none it can use or the heap is full; with this set, creation
     * fails instead. With host access it asks for memory that is both device local and host visible - the BAR
     * window on a discrete GPU - which not every device has. Buffer::Create then reports
     * ErrorCode::FeatureNotSupported when the device has no such memory and ErrorCode::OutOfMemory when it has
     * and the heap is full.
     */
    bool require_device_local = false;
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
     *         buffer cannot do - a device address without the feature, or initial data for memory the host
     *         cannot write - ErrorCode::OutOfBounds for initial data that does not fit, or whatever the
     *         failing allocation maps to.
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
     * Where the memory landed. BufferDesc::host_access is a request the allocator weighs against what the
     * device has: SequentialWrite may land in VRAM the host can map or in system memory, and this is how to
     * tell which. Empty for an empty buffer.
     */
    [[nodiscard]] Opal::Optional<MemoryInfo> GetMemoryInfo() const;

    /**
     * Write data into the buffer at the given offset. Non-coherent memory is flushed, so the write is visible
     * to the device once this returns.
     * @return ErrorCode::Success, ErrorCode::OutOfBounds when the write does not fit, ErrorCode::InvalidArgument
     *         when the memory is not one the host can write, or whatever the failing map maps to.
     */
    ErrorCode Update(Opal::ArrayView<const u8> data, size_t offset = 0) const;

    /**
     * Read data out of the buffer at the given offset, filling the whole view. Non-coherent memory is
     * invalidated first, so what the device wrote is what this returns.
     * @return ErrorCode::Success, ErrorCode::OutOfBounds when the read does not fit, ErrorCode::InvalidArgument
     *         when the buffer was not created with HostAccess::Random - reading write-combined memory works
     *         and is slow enough to be a bug - or whatever the failing map maps to.
     */
    ErrorCode Read(Opal::ArrayView<u8> data, size_t offset = 0) const;

private:
    /** Make a host write to the given range visible to the device. Does nothing on coherent memory. */
    ErrorCode Flush(size_t offset, size_t size) const;

    /** Make a device write to the given range visible to the host. Does nothing on coherent memory. */
    ErrorCode Invalidate(size_t offset, size_t size) const;

    BufferDesc m_desc;
    Opal::Ref<const Device> m_device;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceAddress m_device_address = 0;
    void* m_mapped_memory = nullptr;
};

}  // namespace Rndr::Forge