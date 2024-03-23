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

}  // namespace Rndr
