#include "rndr/canvas/mesh.hpp"

#include "glad/glad.h"

#include "rndr/exception.hpp"
#include "rndr/trace.hpp"

namespace
{

Rndr::u32 FormatByteSize(Rndr::Canvas::Format format)
{
    switch (format)
    {
        case Rndr::Canvas::Format::Float1:
            return 4;
        case Rndr::Canvas::Format::Float2:
            return 8;
        case Rndr::Canvas::Format::Float3:
            return 12;
        case Rndr::Canvas::Format::Float4:
            return 16;
        case Rndr::Canvas::Format::Int1:
            return 4;
        case Rndr::Canvas::Format::Int2:
            return 8;
        case Rndr::Canvas::Format::Int3:
            return 12;
        case Rndr::Canvas::Format::Int4:
            return 16;
        default:
            return 0;
    }
}

GLint FormatComponentCount(Rndr::Canvas::Format format)
{
    switch (format)
    {
        case Rndr::Canvas::Format::Float1:
        case Rndr::Canvas::Format::Int1:
            return 1;
        case Rndr::Canvas::Format::Float2:
        case Rndr::Canvas::Format::Int2:
            return 2;
        case Rndr::Canvas::Format::Float3:
        case Rndr::Canvas::Format::Int3:
            return 3;
        case Rndr::Canvas::Format::Float4:
        case Rndr::Canvas::Format::Int4:
            return 4;
        default:
            return 0;
    }
}

bool IsIntegerFormat(Rndr::Canvas::Format format)
{
    switch (format)
    {
        case Rndr::Canvas::Format::Int1:
        case Rndr::Canvas::Format::Int2:
        case Rndr::Canvas::Format::Int3:
        case Rndr::Canvas::Format::Int4:
            return true;
        default:
            return false;
    }
}

}  // namespace

Rndr::Canvas::Mesh::Mesh(const VertexLayout& layout, Opal::ArrayView<const u8> vertex_data, Opal::ArrayView<const u8> index_data,
                         Opal::StringUtf8 debug_name)
    : m_debug_name(std::move(debug_name))
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Mesh::Mesh");

    if (!layout.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Vertex layout is invalid!");
    }
    if (vertex_data.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Vertex data is empty!");
    }
    if (index_data.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Index data is empty!");
    }

    const u32 stride = layout.GetStride();
    if (stride == 0 || vertex_data.GetSize() % stride != 0)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Vertex data size is not a multiple of the layout stride!");
    }
    if (index_data.GetSize() % sizeof(u32) != 0)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Index data size is not a multiple of 4 bytes!");
    }

    m_layout = layout.Clone();

    m_vertex_count = static_cast<u32>(vertex_data.GetSize() / stride);
    m_index_count = static_cast<u32>(index_data.GetSize() / sizeof(u32));

    Opal::StringUtf8 vertex_buffer_name = m_debug_name + " - Vertex Buffer";
    Opal::StringUtf8 index_buffer_name = m_debug_name + " - Index Buffer";
    m_vertex_buffer = Buffer(BufferUsage::Vertex, vertex_data.GetSize(), 0, vertex_data, std::move(vertex_buffer_name));
    m_index_buffer = Buffer(BufferUsage::Index, index_data.GetSize(), 0, index_data, std::move(index_buffer_name));

    // Store CPU-side copies for Clone().
    m_vertex_data.Resize(vertex_data.GetSize());
    memcpy(m_vertex_data.GetData(), vertex_data.GetData(), vertex_data.GetSize());
    m_index_data.Resize(index_data.GetSize());
    memcpy(m_index_data.GetData(), index_data.GetData(), index_data.GetSize());

    SetupVAO();
}

Rndr::Canvas::Mesh::Mesh(const VertexLayout& layout, i32 max_vertex_count, i32 max_index_count, Opal::StringUtf8 debug_name)
    : m_debug_name(std::move(debug_name)), m_max_vertex_count(max_vertex_count), m_max_index_count(max_index_count)
{
    if (!layout.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Vertex layout is invalid!");
    }
    m_layout = layout.Clone();
    Opal::StringUtf8 vertex_buffer_name = m_debug_name + " - Vertex Buffer";
    Opal::StringUtf8 index_buffer_name = m_debug_name + " - Index Buffer";
    m_vertex_buffer = Buffer(BufferUsage::Vertex, max_vertex_count * layout.GetStride(), 0, {}, std::move(vertex_buffer_name));
    m_index_buffer = Buffer(BufferUsage::Index, max_index_count * sizeof(i32), 0, {}, std::move(index_buffer_name));

    SetupVAO();
}

Rndr::Canvas::Mesh::~Mesh()
{
    Destroy();
}

Rndr::Canvas::Mesh::Mesh(Mesh&& other) noexcept
    : m_debug_name(std::move(other.m_debug_name)),
      m_vao(other.m_vao),
      m_vertex_buffer(std::move(other.m_vertex_buffer)),
      m_index_buffer(std::move(other.m_index_buffer)),
      m_vertex_count(other.m_vertex_count),
      m_index_count(other.m_index_count),
      m_max_vertex_count(other.m_max_vertex_count),
      m_max_index_count(other.m_max_index_count),
      m_layout(std::move(other.m_layout)),
      m_vertex_data(std::move(other.m_vertex_data)),
      m_index_data(std::move(other.m_index_data)),
      m_dirty(other.m_dirty)
{
    other.m_vao = 0;
    other.m_vertex_count = 0;
    other.m_index_count = 0;
    other.m_max_vertex_count = 0;
    other.m_max_index_count = 0;
}

Rndr::Canvas::Mesh& Rndr::Canvas::Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_debug_name = std::move(other.m_debug_name);
        m_vao = other.m_vao;
        m_vertex_buffer = std::move(other.m_vertex_buffer);
        m_index_buffer = std::move(other.m_index_buffer);
        m_vertex_count = other.m_vertex_count;
        m_index_count = other.m_index_count;
        m_max_vertex_count = other.m_max_vertex_count;
        m_max_index_count = other.m_max_index_count;
        m_layout = std::move(other.m_layout);
        m_vertex_data = std::move(other.m_vertex_data);
        m_index_data = std::move(other.m_index_data);
        m_dirty = other.m_dirty;
        other.m_vao = 0;
        other.m_vertex_count = 0;
        other.m_index_count = 0;
        other.m_max_vertex_count = 0;
        other.m_max_index_count = 0;
    }
    return *this;
}

Rndr::Canvas::Mesh Rndr::Canvas::Mesh::Clone() const
{
    if (!IsValid())
    {
        return {};
    }
    return Mesh(m_layout, m_vertex_data, m_index_data);
}

void Rndr::Canvas::Mesh::Destroy()
{
    m_index_buffer.Destroy();
    m_vertex_buffer.Destroy();
    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    m_vertex_count = 0;
    m_index_count = 0;
    m_max_vertex_count = 0;
    m_max_index_count = 0;
    m_layout = VertexLayout();
    m_vertex_data.Clear();
    m_index_data.Clear();
}

void Rndr::Canvas::Mesh::Upload()
{
    if (m_dirty && (!m_vertex_data.IsEmpty() || !m_index_data.IsEmpty()))
    {
        m_dirty = false;
        m_vertex_buffer.Update(m_vertex_data);
        m_index_buffer.Update(m_index_data);
    }
}

void Rndr::Canvas::Mesh::Append(Opal::ArrayView<const u8> vertex_data, Opal::ArrayView<const u8> index_data)
{
    if (vertex_data.IsEmpty() || index_data.IsEmpty())
    {
        return;
    }
    m_vertex_data.Append(vertex_data);
    m_index_data.Append(index_data);
    m_vertex_count += static_cast<u32>(vertex_data.GetSize()) / m_layout.GetStride();
    m_index_count += static_cast<u32>(index_data.GetSize() / sizeof(u32));
    m_dirty = true;
}

void Rndr::Canvas::Mesh::Clear()
{
    m_vertex_data.Clear();
    m_index_data.Clear();
    m_vertex_count = 0;
    m_index_count = 0;
    m_dirty = true;
}

bool Rndr::Canvas::Mesh::IsValid() const
{
    return m_vao != 0;
}

Rndr::u32 Rndr::Canvas::Mesh::GetNativeHandle() const
{
    return m_vao;
}

Rndr::u32 Rndr::Canvas::Mesh::GetVertexCount() const
{
    return m_vertex_count;
}

Rndr::u32 Rndr::Canvas::Mesh::GetIndexCount() const
{
    return m_index_count;
}

bool Rndr::Canvas::Mesh::HasIndices() const
{
    return m_index_count > 0;
}

const Rndr::Canvas::VertexLayout& Rndr::Canvas::Mesh::GetVertexLayout() const
{
    return m_layout;
}

void Rndr::Canvas::Mesh::SetupVAO()
{
    // Create VAO.
    glCreateVertexArrays(1, &m_vao);
    if (m_vao == 0)
    {
        throw GraphicsAPIException(0, "Failed to create GL vertex array!");
    }

    // Bind VBO to VAO at binding point 0.
    glVertexArrayVertexBuffer(m_vao, 0, m_vertex_buffer.GetNativeHandle(), 0, static_cast<GLsizei>(m_layout.GetStride()));

    // Bind IBO to VAO.
    glVertexArrayElementBuffer(m_vao, m_index_buffer.GetNativeHandle());

    // Set up vertex attributes.
    u32 offset = 0;
    for (u32 i = 0; i < m_layout.GetAttributeCount(); ++i)
    {
        const VertexLayout::Entry& entry = m_layout.GetAttribute(i);
        const GLint components = FormatComponentCount(entry.format);

        glEnableVertexArrayAttrib(m_vao, i);
        if (IsIntegerFormat(entry.format))
        {
            glVertexArrayAttribIFormat(m_vao, i, components, GL_INT, offset);
        }
        else
        {
            glVertexArrayAttribFormat(m_vao, i, components, GL_FLOAT, GL_FALSE, offset);
        }
        glVertexArrayAttribBinding(m_vao, i, 0);

        offset += FormatByteSize(entry.format);
    }
    const Opal::StringUtf8 vao_debug_name = m_debug_name + " - Vertex Array";
    glObjectLabel(GL_VERTEX_ARRAY, m_vao, static_cast<GLsizei>(vao_debug_name.GetSize()), *vao_debug_name);
}
