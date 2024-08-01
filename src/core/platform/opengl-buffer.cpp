#include "rndr/core/platform/opengl-buffer.h"

#include <glad/glad.h>

#include "core/platform/opengl-helpers.h"

#include "rndr/core/platform/opengl-graphics-context.h"
#include "rndr/core/trace.h"

Rndr::Buffer::Buffer(const GraphicsContext& graphics_context, const BufferDesc& desc, const Opal::Span<const u8>& init_data) : m_desc(desc)
{
    Initialize(graphics_context, desc, init_data);
}

Rndr::Buffer::~Buffer()
{
    Destroy();
}

Rndr::Buffer::Buffer(Buffer&& other) noexcept : m_desc(other.m_desc), m_native_buffer(other.m_native_buffer)
{
    other.m_native_buffer = k_invalid_opengl_object;
}

Rndr::ErrorCode Rndr::Buffer::Initialize(const Rndr::GraphicsContext& graphics_context, const Rndr::BufferDesc& desc,
                                         const Opal::Span<const u8>& init_data)
{
    RNDR_UNUSED(graphics_context);

    RNDR_CPU_EVENT_SCOPED("Create Buffer");

    if (desc.type >= BufferType::EnumCount)
    {
        RNDR_LOG_ERROR("Buffer::Initialize: Failed, invalid buffer type %d!", desc.type);
        return ErrorCode::InvalidArgument;
    }
    if (desc.usage >= Usage::EnumCount)
    {
        RNDR_LOG_ERROR("Buffer::Initialize: Failed, invalid buffer usage %d!", desc.usage);
        return ErrorCode::InvalidArgument;
    }
    if (desc.size == 0)
    {
        RNDR_LOG_ERROR("Buffer::Initialize: Failed, buffer size is 0!");
        return ErrorCode::InvalidArgument;
    }
    if (desc.offset < 0)
    {
        RNDR_LOG_ERROR("Buffer::Initialize: Failed, invalid buffer offset %d!", desc.offset);
        return ErrorCode::InvalidArgument;
    }
    if (desc.stride < 0)
    {
        RNDR_LOG_ERROR("Buffer::Initialize: Failed, invalid buffer stride %d!", desc.stride);
        return ErrorCode::InvalidArgument;
    }

    glCreateBuffers(1, &m_native_buffer);
    const GLenum buffer_usage = FromUsageToOpenGL(desc.usage);
    glNamedBufferStorage(m_native_buffer, static_cast<i64>(desc.size), init_data.GetData(), buffer_usage);
    const GLenum error_code = glad_glGetError();
    switch (error_code)
    {
        case GL_INVALID_VALUE:
            RNDR_LOG_ERROR("Buffer::Initialize: Failed, invalid buffer size %d!", desc.size);
            return ErrorCode::InvalidArgument;
        case GL_INVALID_OPERATION:
            RNDR_LOG_ERROR("Buffer::Initialize: Failed, specified buffer is invalid!");
            return ErrorCode::InvalidArgument;
        case GL_OUT_OF_MEMORY:
            RNDR_LOG_ERROR("Buffer::Initialize: Failed, out of memory!");
            return ErrorCode::OutOfMemory;
    }
    RNDR_LOG_DEBUG("Buffer::Initialize: opengl id: %u, type: %s, usage: %s, size: %d, stride: %d, offset: %d", m_native_buffer,
                   FromBufferTypeToString(m_desc.type).GetData(), FromOpenGLUsageToString(buffer_usage).GetData(), desc.size, m_desc.stride,
                   m_desc.offset);
    return ErrorCode::Success;
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
        RNDR_LOG_DEBUG("Buffer::Destroy: opengl id: %u", m_native_buffer);
        glDeleteBuffers(1, &m_native_buffer);
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
