#pragma once

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
     */
    explicit Buffer(BufferUsage usage, u64 size);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    [[nodiscard]] Buffer Clone() const;
    void Destroy();

    /**
     * Upload data to the buffer.
     * @param data Pointer to the source data.
     * @param size Size of the data in bytes.
     */
    void Update(const void* data, u64 size);

    [[nodiscard]] BufferUsage GetUsage() const;
    [[nodiscard]] u64 GetSize() const;
    [[nodiscard]] bool IsValid() const;

private:
    BufferUsage m_usage = BufferUsage::Uniform;
    u64 m_size = 0;
    u32 m_handle = 0;
};

}  // namespace Canvas
}  // namespace Rndr
