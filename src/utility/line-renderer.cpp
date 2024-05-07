#include "rndr/utility/line-renderer.h"

#include "rndr/core/file.h"
#include "rndr/utility/input-layout-builder.h"
#include "rndr/utility/cpu-tracer.h"

Rndr::LineRenderer::LineRenderer(const Rndr::String& name, const Rndr::RendererBaseDesc& desc) : RendererBase(name, desc)
{
    RNDR_TRACE_SCOPED(LineRenderer::LineRenderer);

    const String vertex_shader_code = File::ReadShader(RNDR_CORE_ASSETS_DIR, "lines.vert");
    const String pixel_shader_code = File::ReadShader(RNDR_CORE_ASSETS_DIR, "lines.frag");

    m_vertex_shader = RNDR_MAKE_SCOPED(Shader, m_desc.graphics_context, {.type = ShaderType::Vertex, .source = vertex_shader_code});
    if (!m_vertex_shader->IsValid())
    {
        RNDR_LOG_ERROR("Failed to create vertex shader for LineRenderer");
        return;
    }
    m_fragment_shader = RNDR_MAKE_SCOPED(Shader, m_desc.graphics_context, {.type = ShaderType::Fragment, .source = pixel_shader_code});
    if (!m_fragment_shader->IsValid())
    {
        RNDR_LOG_ERROR("Failed to create fragment shader for LineRenderer");
        return;
    }

    constexpr u64 k_stride = sizeof(VertexData);
    m_vertex_buffer =
        RNDR_MAKE_SCOPED(Buffer, m_desc.graphics_context,
                         {.type = Rndr::BufferType::ShaderStorage, .usage = Usage::Dynamic, .size = k_vertex_data_size, .stride = k_stride},
                         ToConstByteSpan(m_vertex_data));
    if (!m_vertex_buffer->IsValid())
    {
        RNDR_LOG_ERROR("Failed to create vertex buffer for LineRenderer");
        return;
    }

    InputLayoutBuilder builder;
    const Rndr::InputLayoutDesc input_layout_desc = builder.AddVertexBuffer(*m_vertex_buffer, 1, Rndr::DataRepetition::PerVertex).Build();
    m_pipeline = RNDR_MAKE_SCOPED(
        Pipeline, m_desc.graphics_context,
        PipelineDesc{.vertex_shader = m_vertex_shader.get(),
                     .pixel_shader = m_fragment_shader.get(),
                     .input_layout = input_layout_desc,
                     .rasterizer = {.fill_mode = Rndr::FillMode::Wireframe, .depth_bias = -1.0, .slope_scaled_depth_bias = -1.0},
                     .depth_stencil = {.is_depth_enabled = true}});
    if (!m_pipeline->IsValid())
    {
        RNDR_LOG_ERROR("Failed to create pipeline for LineRenderer");
        return;
    }

    Matrix4x4f identity_matrix;
    m_constant_buffer = RNDR_MAKE_SCOPED(Buffer, m_desc.graphics_context,
                                         {.type = Rndr::BufferType::Constant,
                                          .usage = Rndr::Usage::Dynamic,
                                          .size = sizeof(Matrix4x4f),
                                          .stride = sizeof(Matrix4x4f)},
                                         Rndr::ToConstByteSpan(identity_matrix));
    if (!m_constant_buffer->IsValid())
    {
        RNDR_LOG_ERROR("Failed to create constant buffer for LineRenderer");
        return;
    }

    m_vertex_data.Reserve(k_max_lines_count * 2);
}

void Rndr::LineRenderer::AddLine(const Point3f& start, const Point3f& end, const Vector4f& color)
{
    if (m_vertex_data.GetSize() >= k_max_lines_count * 2)
    {
        RNDR_LOG_ERROR("LineRenderer reached maximum number of lines");
        return;
    }

    m_vertex_data.PushBack({start, color});
    m_vertex_data.PushBack({end, color});
}

void Rndr::LineRenderer::SetCameraTransform(const Matrix4x4f& transform)
{
    m_desc.graphics_context->Update(*m_constant_buffer, ToConstByteSpan(transform));
}

bool Rndr::LineRenderer::Render()
{
    RNDR_TRACE_SCOPED(LineRenderer::Render);

    if (m_vertex_data.IsEmpty())
    {
        return true;
    }

    m_desc.graphics_context->Bind(*m_desc.swap_chain);
    m_desc.graphics_context->Bind(*m_pipeline);
    m_desc.graphics_context->Bind(*m_constant_buffer, 0);

    m_desc.graphics_context->Update(*m_vertex_buffer, ToConstByteSpan(m_vertex_data));
    const i32 vertex_count = static_cast<i32>(m_vertex_data.GetSize());
    m_desc.graphics_context->DrawVertices(PrimitiveTopology::Line, vertex_count);

    m_vertex_data.Clear();
    return true;
}
