#include "rndr/canvas/buffer.hpp"

#include "glad/glad.h"

#include "rndr/canvas/gl-result.hpp"
#include "rndr/log.hpp"
#include "rndr/trace.hpp"

Opal::Expected<Rndr::Canvas::Buffer, Rndr::ErrorCode> Rndr::Canvas::Buffer::Create(BufferUsage usage, u64 size, u64 offset,
                                                                                   const Opal::ArrayView<const u8>& init_data,
                                                                                   Opal::StringUtf8 name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Buffer::Create");
    using ResultType = Opal::Expected<Buffer, ErrorCode>;

    Buffer buffer;
    buffer.m_usage = usage;
    buffer.m_size = size;
    buffer.m_offset = offset;
    buffer.m_name = std::move(name);

    glCreateBuffers(1, &buffer.m_handle);
    if (buffer.m_handle == 0)
    {
        RNDR_LOG_ERROR("Canvas: Failed to create GL buffer");
        return ResultType(ErrorCode::GraphicsAPIError);
    }

    const void* data = init_data.IsEmpty() ? nullptr : init_data.GetData();
    glNamedBufferStorage(buffer.m_handle, static_cast<GLsizeiptr>(size), data, GL_DYNAMIC_STORAGE_BIT);
    RNDR_CANVAS_GL_CHECK_EXPECTED("glNamedBufferStorage", ResultType);

    if (!buffer.m_name.IsEmpty())
    {
        glObjectLabel(GL_BUFFER, buffer.m_handle, static_cast<GLsizei>(buffer.m_name.GetSize()), buffer.m_name.GetData());
        RNDR_LOG_INFO("Created buffer '{}' with native id {}", *buffer.m_name, buffer.m_handle);
    }
    return ResultType(std::move(buffer));
}

Rndr::Canvas::Buffer::~Buffer()
{
    Destroy();
}

Rndr::Canvas::Buffer::Buffer(Buffer&& other) noexcept
    : m_usage(other.m_usage), m_size(other.m_size), m_offset(other.m_offset), m_handle(other.m_handle), m_name(std::move(other.m_name))
{
    other.m_handle = 0;
    other.m_size = 0;
    other.m_offset = 0;
}

Rndr::Canvas::Buffer& Rndr::Canvas::Buffer::operator=(Buffer&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_usage = other.m_usage;
        m_size = other.m_size;
        m_offset = other.m_offset;
        m_handle = other.m_handle;
        m_name = std::move(other.m_name);
        other.m_handle = 0;
        other.m_size = 0;
        other.m_offset = 0;
    }
    return *this;
}

Opal::Expected<Rndr::Canvas::Buffer, Rndr::ErrorCode> Rndr::Canvas::Buffer::Clone(Opal::AllocatorBase* allocator) const
{
    using ResultType = Opal::Expected<Buffer, ErrorCode>;
    if (!IsValid())
    {
        RNDR_LOG_ERROR("Canvas: Cannot clone an invalid buffer");
        return ResultType(ErrorCode::InvalidArgument);
    }
    ResultType clone_result = Create(m_usage, m_size, m_offset, {}, m_name.Clone(allocator));
    if (!clone_result.HasValue())
    {
        return clone_result;
    }
    glCopyNamedBufferSubData(m_handle, clone_result.GetValue().m_handle, 0, 0, static_cast<GLsizeiptr>(m_size));
    RNDR_CANVAS_GL_CHECK_EXPECTED("glCopyNamedBufferSubData", ResultType);
    return clone_result;
}

void Rndr::Canvas::Buffer::Destroy()
{
    if (m_handle != 0)
    {
        glDeleteBuffers(1, &m_handle);
        m_handle = 0;
        m_size = 0;
        m_offset = 0;
    }
}

Rndr::ErrorCode Rndr::Canvas::Buffer::Update(const Opal::ArrayView<const u8>& data) const
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Buffer::Update");

    if (m_handle == 0)
    {
        RNDR_LOG_ERROR("Canvas: Cannot update an invalid buffer");
        return ErrorCode::InvalidArgument;
    }
    if (data.GetSize() > m_size)
    {
        RNDR_LOG_ERROR("Canvas: Update size exceeds buffer size");
        return ErrorCode::OutOfBounds;
    }

    glNamedBufferSubData(m_handle, static_cast<GLintptr>(m_offset), static_cast<GLsizeiptr>(data.GetSize()), data.GetData());
    RNDR_CANVAS_GL_CHECK("glNamedBufferSubData");
    return ErrorCode::Success;
}

Rndr::Canvas::BufferUsage Rndr::Canvas::Buffer::GetUsage() const
{
    return m_usage;
}

Rndr::u64 Rndr::Canvas::Buffer::GetSize() const
{
    return m_size;
}

Rndr::u64 Rndr::Canvas::Buffer::GetOffset() const
{
    return m_offset;
}

const Opal::StringUtf8& Rndr::Canvas::Buffer::GetName() const
{
    return m_name;
}

Rndr::u32 Rndr::Canvas::Buffer::GetNativeHandle() const
{
    return m_handle;
}

bool Rndr::Canvas::Buffer::IsValid() const
{
    return m_handle != 0;
}
