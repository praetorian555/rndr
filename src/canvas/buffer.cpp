#include "rndr/canvas/buffer.hpp"

#include "glad/glad.h"

#include "rndr/exception.hpp"
#include "rndr/trace.hpp"

Rndr::Canvas::Buffer::Buffer(BufferUsage usage, u64 size, u64 offset, const Opal::ArrayView<const u8>& init_data,
                             Opal::StringUtf8 name)
    : m_usage(usage), m_size(size), m_offset(offset), m_name(std::move(name))
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Buffer::Buffer");

    glCreateBuffers(1, &m_handle);
    if (m_handle == 0)
    {
        throw GraphicsAPIException(0, "Failed to create GL buffer!");
    }

    const void* data = init_data.IsEmpty() ? nullptr : init_data.GetData();
    glNamedBufferStorage(m_handle, static_cast<GLsizeiptr>(m_size), data, GL_DYNAMIC_STORAGE_BIT);

    const GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        glDeleteBuffers(1, &m_handle);
        m_handle = 0;
        throw GraphicsAPIException(err, "Failed to allocate GL buffer storage!");
    }

    if (!m_name.IsEmpty())
    {
        glObjectLabel(GL_BUFFER, m_handle, static_cast<GLsizei>(m_name.GetSize()), m_name.GetData());
    }
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

Rndr::Canvas::Buffer Rndr::Canvas::Buffer::Clone(Opal::AllocatorBase* allocator) const
{
    if (!IsValid())
    {
        return {};
    }
    Buffer clone(m_usage, m_size, m_offset, {}, m_name.Clone(allocator));
    glCopyNamedBufferSubData(m_handle, clone.m_handle, 0, 0, static_cast<GLsizeiptr>(m_size));
    return clone;
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

void Rndr::Canvas::Buffer::Update(const Opal::ArrayView<const u8>& data) const
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Buffer::Update");

    if (m_handle == 0)
    {
        throw GraphicsAPIException(0, "Cannot update an invalid buffer!");
    }
    if (data.GetSize() > m_size)
    {
        throw GraphicsAPIException(0, "Update size exceeds buffer size!");
    }

    glNamedBufferSubData(m_handle, static_cast<GLintptr>(m_offset), static_cast<GLsizeiptr>(data.GetSize()), data.GetData());
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
