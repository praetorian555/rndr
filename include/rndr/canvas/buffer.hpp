#pragma once

#include "opal/container/array-view.h"
#include "opal/container/string.h"

#include "rndr/types.hpp"

namespace Rndr
{
namespace Canvas
{

/** Describes the intended usage of a Canvas buffer. */
enum class BufferUsage : u8
{
    Vertex,
    Index,
    Uniform,
    Storage,
    EnumCount
};

/**
 * GPU data buffer. Updates happen on the buffer object directly, not on the DrawList, to avoid
 * ambiguity about when updates happen.
 */
class Buffer
{
public:
    Buffer() = default;

    /**
     * Create a GPU buffer.
     * @param usage Intended usage of the buffer.
     * @param size Size in bytes.
     * @param offset Byte offset into the buffer for binding.
     * @param init_data Optional initial data to upload.
     * @param name Debug name for GPU debugging tools.
     */
    explicit Buffer(BufferUsage usage, u64 size, u64 offset = 0, const Opal::ArrayView<const u8>& init_data = {},
                    Opal::StringUtf8 name = {});
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    [[nodiscard]] Buffer Clone(Opal::AllocatorBase* allocator = nullptr) const;
    void Destroy();

    /**
     * Upload data to the buffer.
     * @param data Data to upload.
     */
    void Update(const Opal::ArrayView<const u8>& data) const;

    [[nodiscard]] BufferUsage GetUsage() const;
    [[nodiscard]] u64 GetSize() const;
    [[nodiscard]] u64 GetOffset() const;
    [[nodiscard]] const Opal::StringUtf8& GetName() const;
    [[nodiscard]] bool IsValid() const;

private:
    BufferUsage m_usage = BufferUsage::Uniform;
    u64 m_size = 0;
    u64 m_offset = 0;
    u32 m_handle = 0;
    Opal::StringUtf8 m_name;
};

}  // namespace Canvas
}  // namespace Rndr
