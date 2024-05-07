#pragma once

#include "rndr/core/definitions.h"
#include "rndr/core/graphics-types.h"
#include "rndr/core/platform/opengl-forward-def.h"

namespace Rndr
{

class GraphicsContext;

/**
 * Represents a linear array of memory on the GPU.
 */
class Buffer
{
public:
    /**
     * Default constructor. Creates an invalid buffer.
     */
    Buffer() = default;

    /**
     * Create a new buffer.
     * @param graphics_context The graphics context to create the buffer with.
     * @param desc The description of the buffer to create.
     * @param init_data The initial data to fill the buffer with. If empty, the buffer will be filled with zeros. Default is empty.
     */
    Buffer(const GraphicsContext& graphics_context, const BufferDesc& desc, const ConstByteSpan& init_data = ConstByteSpan{});

    /**
     * Create a new buffer with initial data.
     * @tparam DataType The type of the data to initialize the buffer with.
     * @param graphics_context The graphics context to create the buffer with.
     * @param init_data The initial data to fill the buffer with.
     * @param type The type of the buffer. Default is Vertex.
     * @param usage The usage of the buffer. Default is Default.
     * @param offset The offset in the buffer to start writing the data. Default is 0.
     */
    template <typename DataType>
    Buffer(const GraphicsContext& graphics_context, const Span<const DataType>& init_data, BufferType type, Usage usage = Rndr::Usage::Default,
           uint32_t offset = 0);

    ~Buffer();
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const BufferDesc& GetDesc() const;
    [[nodiscard]] GLuint GetNativeBuffer() const;

private:
    BufferDesc m_desc;
    GLuint m_native_buffer = k_invalid_opengl_object;
};

template <typename DataType>
Rndr::Buffer::Buffer(const Rndr::GraphicsContext& graphics_context, const Rndr::Span<const DataType>& init_data, Rndr::BufferType type,
                     Rndr::Usage usage, uint32_t offset)
    : Buffer(graphics_context,
             BufferDesc{.type = type,
                        .usage = usage,
                        .size = ((uint32_t)init_data.size()) * sizeof(DataType),
                        .stride = sizeof(DataType),
                        .offset = offset * static_cast<uint32_t>(sizeof(DataType))},
             ToConstByteSpan(init_data))
{
}

}  // namespace Rndr
