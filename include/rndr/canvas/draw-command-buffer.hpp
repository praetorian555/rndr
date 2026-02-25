#pragma once

#include "rndr/types.hpp"

namespace Rndr
{
namespace Canvas
{

/** Draw command for non-indexed geometry, used with indirect drawing. */
struct DrawCommand
{
    u32 vertex_count = 0;
    u32 instance_count = 1;
    u32 first_vertex = 0;
    u32 first_instance = 0;
};

/** Draw command for indexed geometry, used with indirect drawing. */
struct DrawIndexedCommand
{
    u32 index_count = 0;
    u32 instance_count = 1;
    u32 first_index = 0;
    i32 vertex_offset = 0;
    u32 first_instance = 0;
};

/**
 * Fixed-layout buffer for indirect draw commands. Templated on command type for compile-time
 * safety. Supports DrawCommand for non-indexed and DrawIndexedCommand for indexed geometry.
 */
template<typename T>
class DrawCommandBuffer
{
public:
    DrawCommandBuffer() = default;

    /**
     * Create a draw command buffer.
     * @param max_count Maximum number of commands the buffer can hold.
     */
    explicit DrawCommandBuffer(u32 max_count);
    ~DrawCommandBuffer();

    DrawCommandBuffer(const DrawCommandBuffer&) = delete;
    DrawCommandBuffer& operator=(const DrawCommandBuffer&) = delete;
    DrawCommandBuffer(DrawCommandBuffer&& other) noexcept;
    DrawCommandBuffer& operator=(DrawCommandBuffer&& other) noexcept;

    [[nodiscard]] DrawCommandBuffer Clone() const;
    void Destroy();

    [[nodiscard]] u32 GetMaxCount() const;
    [[nodiscard]] bool IsValid() const;

private:
    u32 m_max_count = 0;
    u32 m_handle = 0;
};

}  // namespace Canvas
}  // namespace Rndr
