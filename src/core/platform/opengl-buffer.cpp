#include "rndr/core/platform/opengl-buffer.h"

#include <glad/glad.h>

#include "core/platform/opengl-helpers.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/platform/opengl-graphics-context.h"
#include "rndr/utility/cpu-tracer.h"

Rndr::Buffer::Buffer(const GraphicsContext& graphics_context, const BufferDesc& desc, const Opal::Span<const u8>& init_data) : m_desc(desc)
{
    RNDR_TRACE_SCOPED(Create Buffer);

    RNDR_UNUSED(graphics_context);
    glCreateBuffers(1, &m_native_buffer);
    RNDR_ASSERT_OPENGL();
    const GLenum buffer_usage = FromUsageToOpenGL(desc.usage);
    glNamedBufferStorage(m_native_buffer, desc.size, init_data.GetData(), buffer_usage);
    RNDR_ASSERT_OPENGL();
    RNDR_LOG_DEBUG("Created %s buffer %u, size: %d, stride: %d, usage %s", FromBufferTypeToString(m_desc.type).c_str(), m_native_buffer,
                   desc.size, m_desc.stride, FromOpenGLUsageToString(buffer_usage).c_str());
}

Rndr::Buffer::~Buffer()
{
    Destroy();
}

Rndr::Buffer::Buffer(Buffer&& other) noexcept : m_desc(other.m_desc), m_native_buffer(other.m_native_buffer)
{
    other.m_native_buffer = k_invalid_opengl_object;
}

Rndr::Buffer& Rndr::Buffer::operator=(Buffer&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_native_buffer = other.m_native_buffer;
        other.m_native_buffer = k_invalid_opengl_object;
    }
    return *this;
}

void Rndr::Buffer::Destroy()
{
    if (m_native_buffer != k_invalid_opengl_object)
    {
        glDeleteBuffers(1, &m_native_buffer);
        RNDR_ASSERT_OPENGL();
        m_native_buffer = k_invalid_opengl_object;
    }
}

bool Rndr::Buffer::IsValid() const
{
    return m_native_buffer != k_invalid_opengl_object;
}

const Rndr::BufferDesc& Rndr::Buffer::GetDesc() const
{
    return m_desc;
}

GLuint Rndr::Buffer::GetNativeBuffer() const
{
    return m_native_buffer;
}
