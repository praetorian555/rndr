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

Rndr::Canvas::Mesh::Mesh(const VertexLayout& layout, Opal::ArrayView<const u8> vertex_data, Opal::ArrayView<const u8> index_data)
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

    const u32 stride = layout.GetStride();
    if (stride == 0 || vertex_data.GetSize() % stride != 0)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Vertex data size is not a multiple of the layout stride!");
    }

    m_vertex_count = static_cast<u32>(vertex_data.GetSize() / stride);
    m_layout = layout.Clone();

    // Store CPU-side copies for Clone().
    m_vertex_data.Resize(vertex_data.GetSize());
    memcpy(m_vertex_data.GetData(), vertex_data.GetData(), vertex_data.GetSize());

    if (!index_data.IsEmpty())
    {
        if (index_data.GetSize() % sizeof(u32) != 0)
        {
            throw Opal::InvalidArgumentException(__FUNCTION__, "Index data size is not a multiple of 4 bytes!");
        }
        m_index_count = static_cast<u32>(index_data.GetSize() / sizeof(u32));
        m_index_data.Resize(index_data.GetSize());
        memcpy(m_index_data.GetData(), index_data.GetData(), index_data.GetSize());
    }

    // Create VAO.
    glCreateVertexArrays(1, &m_vao);
    if (m_vao == 0)
    {
        throw GraphicsAPIException(0, "Failed to create GL vertex array!");
    }

    // Create VBO.
    glCreateBuffers(1, &m_vbo);
    if (m_vbo == 0)
    {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
        throw GraphicsAPIException(0, "Failed to create GL vertex buffer!");
    }
    glNamedBufferStorage(m_vbo, static_cast<GLsizeiptr>(vertex_data.GetSize()), vertex_data.GetData(), 0);

    // Bind VBO to VAO at binding point 0.
    glVertexArrayVertexBuffer(m_vao, 0, m_vbo, 0, static_cast<GLsizei>(stride));

    // Set up vertex attributes.
    u32 offset = 0;
    for (u32 i = 0; i < layout.GetAttributeCount(); ++i)
    {
        const VertexLayout::Entry& entry = layout.GetAttribute(i);
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

    // Create IBO if indexed.
    if (m_index_count > 0)
    {
        glCreateBuffers(1, &m_ibo);
        if (m_ibo == 0)
        {
            Destroy();
            throw GraphicsAPIException(0, "Failed to create GL index buffer!");
        }
        glNamedBufferStorage(m_ibo, static_cast<GLsizeiptr>(index_data.GetSize()), index_data.GetData(), 0);
        glVertexArrayElementBuffer(m_vao, m_ibo);
    }
}

Rndr::Canvas::Mesh::~Mesh()
{
    Destroy();
}

Rndr::Canvas::Mesh::Mesh(Mesh&& other) noexcept
    : m_vao(other.m_vao),
      m_vbo(other.m_vbo),
      m_ibo(other.m_ibo),
      m_vertex_count(other.m_vertex_count),
      m_index_count(other.m_index_count),
      m_layout(std::move(other.m_layout)),
      m_vertex_data(std::move(other.m_vertex_data)),
      m_index_data(std::move(other.m_index_data))
{
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ibo = 0;
    other.m_vertex_count = 0;
    other.m_index_count = 0;
}

Rndr::Canvas::Mesh& Rndr::Canvas::Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ibo = other.m_ibo;
        m_vertex_count = other.m_vertex_count;
        m_index_count = other.m_index_count;
        m_layout = std::move(other.m_layout);
        m_vertex_data = std::move(other.m_vertex_data);
        m_index_data = std::move(other.m_index_data);
        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ibo = 0;
        other.m_vertex_count = 0;
        other.m_index_count = 0;
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
    if (m_ibo != 0)
    {
        glDeleteBuffers(1, &m_ibo);
        m_ibo = 0;
    }
    if (m_vbo != 0)
    {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    m_vertex_count = 0;
    m_index_count = 0;
    m_layout = VertexLayout();
    m_vertex_data.Clear();
    m_index_data.Clear();
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
