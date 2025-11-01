#include "shape-2d-renderer.hpp"

#include "rndr/input-layout-builder.hpp"
#include "rndr/projections.hpp"

bool Shape2DRenderer::Init(Rndr::GraphicsContext* gc, i32 fb_width, i32 fb_height)
{
    m_gc = gc;
    m_fb_width = fb_width;
    m_fb_height = fb_height;

    m_per_frame_data_buffer = {*m_gc, Rndr::BufferDesc{.type = Rndr::BufferType::Constant,
                                                       .usage = Rndr::Usage::Dynamic,
                                                       .size = sizeof(Rndr::Matrix4x4f),
                                                       .stride = sizeof(Rndr::Matrix4x4f)}};
    RNDR_ASSERT(m_per_frame_data_buffer.IsValid(), "Failed to initialize per frame buffer!");

    m_vertex_buffer = {*m_gc, Rndr::BufferDesc{.type = Rndr::BufferType::ShaderStorage,
                                               .usage = Rndr::Usage::Dynamic,
                                               .size = k_max_vertex_count * sizeof(VertexData),
                                               .stride = sizeof(VertexData)}};
    RNDR_ASSERT(m_vertex_buffer.IsValid(), "Failed to initialize vertex buffer!");

    m_index_buffer = {*m_gc, Rndr::BufferDesc{.type = Rndr::BufferType::Index,
                                              .usage = Rndr::Usage::Dynamic,
                                              .size = 2 * k_max_vertex_count * sizeof(i32),
                                              .stride = sizeof(i32)}};
    RNDR_ASSERT(m_index_buffer.IsValid(), "Failed to initialize index buffer!");

    const Opal::StringUtf8 vertex_shader_contents = R"(
        #version 460 core

        layout(binding = 0) uniform PerFrameData
        {
            mat4 mvp;
        } per_frame_data;

        struct VertexData
        {
            float pos[2];
            float color[4];
        };
        layout(std430, binding = 0) restrict readonly buffer Vertices
        {
            VertexData vertices[];
        };

        vec4 GetPosition(int i) { return vec4(vertices[i].pos[0], vertices[i].pos[1], 0.0, 1.0); }
        vec4 GetColor(int i) { return vec4(vertices[i].color[0], vertices[i].color[1], vertices[i].color[2], vertices[i].color[3]); }

        layout(location = 0) out vec4 out_color;

        void main()
        {
            gl_Position = per_frame_data.mvp * GetPosition(gl_VertexID);
            out_color = GetColor(gl_VertexID);
        }
    )";

    const Opal::StringUtf8 fragment_shader_contents = R"(
        #version 460 core

        layout(location = 0) in vec4 in_color;

        layout(location = 0) out vec4 out_color;

        void main()
        {
            out_color = in_color;
        }
    )";

    m_vertex_shader = {*m_gc, Rndr::ShaderDesc{.type = Rndr::ShaderType::Vertex, .source = vertex_shader_contents}};
    RNDR_ASSERT(m_vertex_shader.IsValid(), "Failed to initialize vertex shader!");
    m_fragment_shader = {*m_gc, Rndr::ShaderDesc{.type = Rndr::ShaderType::Fragment, .source = fragment_shader_contents}};
    RNDR_ASSERT(m_fragment_shader.IsValid(), "Failed to initialize fragment shader!");

    const Rndr::InputLayoutDesc input_layout = Rndr::InputLayoutBuilder()
                                                   .AddVertexBuffer(m_vertex_buffer, 0, Rndr::DataRepetition::PerVertex)
                                                   .AddIndexBuffer(m_index_buffer)
                                                   .Build();

    m_pipeline = {*m_gc,
                  Rndr::PipelineDesc{.vertex_shader = &m_vertex_shader, .pixel_shader = &m_fragment_shader, .input_layout = input_layout}};
    RNDR_ASSERT(m_pipeline.IsValid(), "Failed to initialize pipeline!");

    return true;
}

void Shape2DRenderer::Destroy()
{
    m_per_frame_data_buffer.Destroy();
    m_vertex_buffer.Destroy();
    m_index_buffer.Destroy();
    m_vertex_shader.Destroy();
    m_fragment_shader.Destroy();
    m_pipeline.Destroy();
}
void Shape2DRenderer::SetFrameBufferSize(i32 width, i32 height)
{
    m_fb_width = width;
    m_fb_height = height;
}

void Shape2DRenderer::Render(f32 delta_seconds, Rndr::CommandList& cmd_list)
{
    RNDR_UNUSED(delta_seconds);

    Rndr::Matrix4x4f projection = Rndr::OrthographicOpenGL(0, m_fb_width, 0, m_fb_height, -1, 1);
    projection = Opal::Transpose(projection);

    cmd_list.CmdUpdateBuffer(m_per_frame_data_buffer, Opal::AsBytes(projection));
    cmd_list.CmdUpdateBuffer(m_vertex_buffer, Opal::AsBytes(m_vertices));
    cmd_list.CmdUpdateBuffer(m_index_buffer, Opal::AsBytes(m_indices));

    cmd_list.CmdBindBuffer(m_per_frame_data_buffer, 0);
    cmd_list.CmdBindPipeline(m_pipeline);

    cmd_list.CmdDrawIndices(Rndr::PrimitiveTopology::Triangle, static_cast<i32>(m_indices.GetSize()));

    m_vertices.Clear();
    m_indices.Clear();
}

void Shape2DRenderer::DrawTriangle(const Rndr::Point2f& a, const Rndr::Point2f& b, const Rndr::Point2f& c, const Rndr::Vector4f& color)
{
    const i32 m_vertex_base = static_cast<i32>(m_vertices.GetSize());
    m_vertices.PushBack({.pos = a, .color = color});
    m_vertices.PushBack({.pos = b, .color = color});
    m_vertices.PushBack({.pos = c, .color = color});

    m_indices.PushBack(m_vertex_base + 0);
    m_indices.PushBack(m_vertex_base + 1);
    m_indices.PushBack(m_vertex_base + 2);
}

void Shape2DRenderer::DrawRect(const Rndr::Point2f& bottom_left, const Rndr::Vector2f& size, const Rndr::Vector4f& color)
{
    const i32 m_vertex_base = static_cast<i32>(m_vertices.GetSize());
    m_vertices.PushBack({.pos = bottom_left, .color = color});
    m_vertices.PushBack({.pos = bottom_left + Rndr::Vector2f{size.x, 0}, .color = color});
    m_vertices.PushBack({.pos = bottom_left + Rndr::Vector2f{size.x, size.y}, .color = color});
    m_vertices.PushBack({.pos = bottom_left + Rndr::Vector2f{0, size.y}, .color = color});

    m_indices.PushBack(m_vertex_base + 0);
    m_indices.PushBack(m_vertex_base + 1);
    m_indices.PushBack(m_vertex_base + 2);
    m_indices.PushBack(m_vertex_base + 0);
    m_indices.PushBack(m_vertex_base + 2);
    m_indices.PushBack(m_vertex_base + 3);
}

void Shape2DRenderer::DrawLine(const Rndr::Point2f& start, const Rndr::Point2f& end, const Rndr::Vector4f& color, f32 thickness)
{
    Rndr::Vector2f dir = end - start;
    dir = Opal::Normalize(dir);
    const Rndr::Vector2f perp(-dir.y, dir.x);

    const i32 m_vertex_base = static_cast<i32>(m_vertices.GetSize());
    m_vertices.PushBack({.pos = start + 0.5f * thickness * perp, .color = color});
    m_vertices.PushBack({.pos = start - 0.5f * thickness * perp, .color = color});
    m_vertices.PushBack({.pos = end - 0.5f * thickness * perp, .color = color});
    m_vertices.PushBack({.pos = end + 0.5f * thickness * perp, .color = color});

    m_indices.PushBack(m_vertex_base + 0);
    m_indices.PushBack(m_vertex_base + 1);
    m_indices.PushBack(m_vertex_base + 2);
    m_indices.PushBack(m_vertex_base + 0);
    m_indices.PushBack(m_vertex_base + 2);
    m_indices.PushBack(m_vertex_base + 3);
}

void Shape2DRenderer::DrawBezierSquare(const Rndr::Point2f& start, const Rndr::Point2f& control, const Rndr::Point2f& end,
                                       const Rndr::Vector4f& color, f32 thickness, i32 segment_count)
{
    Rndr::Point2f curr_start = start;
    for (i32 segment_idx = 0; segment_idx < segment_count; ++segment_idx)
    {
        const f32 t = static_cast<f32>(segment_idx + 1) * (1.0f / static_cast<f32>(segment_count));
        Rndr::Point2f curr_end = Rndr::Point2f::Zero();
        curr_end.x = (1 - t) * (1 - t) * start.x + 2 * (1 - t) * t * control.x + t * t * end.x;
        curr_end.y = (1 - t) * (1 - t) * start.y + 2 * (1 - t) * t * control.y + t * t * end.y;
        DrawLine(curr_start, curr_end, color, thickness);
        curr_start = curr_end;
    }
}

void Shape2DRenderer::DrawBezierCubic(const Rndr::Point2f& start, const Rndr::Point2f& control0, const Rndr::Point2f& control1,
                                      const Rndr::Point2f& end, const Rndr::Vector4f& color, f32 thickness, i32 segment_count)
{
    Rndr::Point2f curr_start = start;
    for (i32 segment_idx = 0; segment_idx < segment_count; ++segment_idx)
    {
        const f32 t = static_cast<f32>(segment_idx + 1) * (1.0f / static_cast<f32>(segment_count));
        Rndr::Point2f curr_end = Rndr::Point2f::Zero();
        curr_end.x = (1 - t) * (1 - t) * (1 - t) * start.x + 3 * (1 - t) * (1 - t) * t * control0.x + 3 * (1 - t) * t * t * control1.x +
                     t * t * t * end.x;
        curr_end.y = (1 - t) * (1 - t) * (1 - t) * start.y + 3 * (1 - t) * (1 - t) * t * control0.y + 3 * (1 - t) * t * t * control1.y +
                     t * t * t * end.y;
        DrawLine(curr_start, curr_end, color, thickness);
        curr_start = curr_end;
    }
}

void Shape2DRenderer::DrawCircle(const Rndr::Point2f& center, f32 radius, const Rndr::Vector4f& color, i32 segment_count)
{
    Rndr::Point2f curr_start = center + Rndr::Vector2f{radius, 0};
    for (i32 segment_idx = 0; segment_idx < segment_count; ++segment_idx)
    {
        const f32 step = 360.0f / static_cast<f32>(segment_count);
        const f32 curr_step_radians = Opal::Radians(static_cast<f32>(segment_idx + 1) * step);
        Rndr::Point2f curr_end{radius * Opal::Cos(curr_step_radians), radius * Opal::Sin(curr_step_radians)};
        curr_end += Rndr::Vector2f{center.x, center.y};
        DrawTriangle(curr_start, curr_end, center, color);
        curr_start = curr_end;
    }
}