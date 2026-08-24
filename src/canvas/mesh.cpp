#include "rndr/canvas/mesh.hpp"

#include "glad/glad.h"

#include "rndr/canvas/gl-result.hpp"
#include "rndr/log.hpp"
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

Opal::Expected<Rndr::Canvas::Mesh, Rndr::ErrorCode> Rndr::Canvas::Mesh::Create(const VertexLayout& layout,
                                                                               Opal::ArrayView<const u8> vertex_data,
                                                                               Opal::ArrayView<const u8> index_data,
                                                                               Opal::StringUtf8 debug_name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Mesh::Create");
    using ResultType = Opal::Expected<Mesh, ErrorCode>;

    if (!layout.IsValid())
    {
        RNDR_LOG_ERROR("Canvas: Vertex layout is invalid");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (vertex_data.IsEmpty())
    {
        RNDR_LOG_ERROR("Canvas: Vertex data is empty");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (index_data.IsEmpty())
    {
        RNDR_LOG_ERROR("Canvas: Index data is empty");
        return ResultType(ErrorCode::InvalidArgument);
    }

    const u32 stride = layout.GetStride();
    if (stride == 0 || vertex_data.GetSize() % stride != 0)
    {
        RNDR_LOG_ERROR("Canvas: Vertex data size is not a multiple of the layout stride");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (index_data.GetSize() % sizeof(u32) != 0)
    {
        RNDR_LOG_ERROR("Canvas: Index data size is not a multiple of 4 bytes");
        return ResultType(ErrorCode::InvalidArgument);
    }

    Mesh mesh;
    mesh.m_debug_name = std::move(debug_name);
    mesh.m_layout = layout.Clone();

    mesh.m_vertex_count = static_cast<u32>(vertex_data.GetSize() / stride);
    mesh.m_index_count = static_cast<u32>(index_data.GetSize() / sizeof(u32));

    Opal::StringUtf8 vertex_buffer_name = mesh.m_debug_name + " - Vertex Buffer";
    Opal::StringUtf8 index_buffer_name = mesh.m_debug_name + " - Index Buffer";
    auto vertex_buffer_result = Buffer::Create(BufferUsage::Vertex, vertex_data.GetSize(), 0, vertex_data, std::move(vertex_buffer_name));
    RNDR_CANVAS_CHECK_EXPECTED(vertex_buffer_result.GetErrorOr(ErrorCode::Success), ResultType);
    mesh.m_vertex_buffer = std::move(vertex_buffer_result).GetValue();
    auto index_buffer_result = Buffer::Create(BufferUsage::Index, index_data.GetSize(), 0, index_data, std::move(index_buffer_name));
    RNDR_CANVAS_CHECK_EXPECTED(index_buffer_result.GetErrorOr(ErrorCode::Success), ResultType);
    mesh.m_index_buffer = std::move(index_buffer_result).GetValue();

    // Store CPU-side copies for Clone().
    mesh.m_vertex_data.Resize(vertex_data.GetSize());
    memcpy(mesh.m_vertex_data.GetData(), vertex_data.GetData(), vertex_data.GetSize());
    mesh.m_index_data.Resize(index_data.GetSize());
    memcpy(mesh.m_index_data.GetData(), index_data.GetData(), index_data.GetSize());

    RNDR_CANVAS_CHECK_EXPECTED(mesh.SetupVAO(), ResultType);
    return ResultType(std::move(mesh));
}

Opal::Expected<Rndr::Canvas::Mesh, Rndr::ErrorCode> Rndr::Canvas::Mesh::Create(const VertexLayout& layout, i32 max_vertex_count,
                                                                               i32 max_index_count, Opal::StringUtf8 debug_name)
{
    using ResultType = Opal::Expected<Mesh, ErrorCode>;

    if (!layout.IsValid())
    {
        RNDR_LOG_ERROR("Canvas: Vertex layout is invalid");
        return ResultType(ErrorCode::InvalidArgument);
    }

    Mesh mesh;
    mesh.m_debug_name = std::move(debug_name);
    mesh.m_max_vertex_count = max_vertex_count;
    mesh.m_max_index_count = max_index_count;
    mesh.m_layout = layout.Clone();
    Opal::StringUtf8 vertex_buffer_name = mesh.m_debug_name + " - Vertex Buffer";
    Opal::StringUtf8 index_buffer_name = mesh.m_debug_name + " - Index Buffer";
    auto vertex_buffer_result =
        Buffer::Create(BufferUsage::Vertex, max_vertex_count * layout.GetStride(), 0, {}, std::move(vertex_buffer_name));
    RNDR_CANVAS_CHECK_EXPECTED(vertex_buffer_result.GetErrorOr(ErrorCode::Success), ResultType);
    mesh.m_vertex_buffer = std::move(vertex_buffer_result).GetValue();
    auto index_buffer_result = Buffer::Create(BufferUsage::Index, max_index_count * sizeof(i32), 0, {}, std::move(index_buffer_name));
    RNDR_CANVAS_CHECK_EXPECTED(index_buffer_result.GetErrorOr(ErrorCode::Success), ResultType);
    mesh.m_index_buffer = std::move(index_buffer_result).GetValue();

    RNDR_CANVAS_CHECK_EXPECTED(mesh.SetupVAO(), ResultType);
    return ResultType(std::move(mesh));
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

Opal::Expected<Rndr::Canvas::Mesh, Rndr::ErrorCode> Rndr::Canvas::Mesh::Clone() const
{
    if (!IsValid())
    {
        RNDR_LOG_ERROR("Canvas: Cannot clone an invalid mesh");
        return Opal::Expected<Mesh, ErrorCode>(ErrorCode::InvalidArgument);
    }
    return Create(m_layout, m_vertex_data, m_index_data);
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
        (void)m_vertex_buffer.Update(m_vertex_data);
        (void)m_index_buffer.Update(m_index_data);
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

Rndr::ErrorCode Rndr::Canvas::Mesh::SetupVAO()
{
    // Create VAO.
    glCreateVertexArrays(1, &m_vao);
    if (m_vao == 0)
    {
        RNDR_LOG_ERROR("Canvas: Failed to create GL vertex array");
        return ErrorCode::GraphicsAPIError;
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
    return ErrorCode::Success;
}
