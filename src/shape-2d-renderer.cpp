#include "../include/rndr/renderers/shape-2d-renderer.hpp"

#include "rndr/input-layout-builder.hpp"
#include "rndr/projections.hpp"

Rndr::Shape2DRenderer::Shape2DRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc, Opal::Ref<FrameBuffer> target)
    : RendererBase(name, desc)
{
    SetFrameBufferTarget(target);

    m_per_frame_data_buffer = {m_desc.graphics_context, Rndr::BufferDesc{.type = Rndr::BufferType::Constant,
                                                                         .usage = Rndr::Usage::Dynamic,
                                                                         .size = sizeof(Rndr::Matrix4x4f),
                                                                         .stride = sizeof(Rndr::Matrix4x4f),
                                                                         .debug_name = "Shape 2D Renderer - Per Frame Buffer"}};
    RNDR_ASSERT(m_per_frame_data_buffer.IsValid(), "Failed to initialize per frame buffer!");

    m_vertex_buffer = {m_desc.graphics_context, Rndr::BufferDesc{.type = Rndr::BufferType::ShaderStorage,
                                                                 .usage = Rndr::Usage::Dynamic,
                                                                 .size = k_max_vertex_count * sizeof(VertexData),
                                                                 .stride = sizeof(VertexData),
                                                                 .debug_name = "Shape 2D Renderer - Vertex Buffer"}};
    RNDR_ASSERT(m_vertex_buffer.IsValid(), "Failed to initialize vertex buffer!");

    m_index_buffer = {m_desc.graphics_context, Rndr::BufferDesc{.type = Rndr::BufferType::Index,
                                                                .usage = Rndr::Usage::Dynamic,
                                                                .size = 2 * k_max_vertex_count * sizeof(i32),
                                                                .stride = sizeof(i32),
                                                                .debug_name = "Shape 2D Renderer - Index Buffer"}};
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

    m_vertex_shader = {m_desc.graphics_context, Rndr::ShaderDesc{.type = Rndr::ShaderType::Vertex,
                                                                 .source = vertex_shader_contents,
                                                                 .debug_name = "Shape 2D Renderer - Vertex Shader"}};
    RNDR_ASSERT(m_vertex_shader.IsValid(), "Failed to initialize vertex shader!");
    m_fragment_shader = {m_desc.graphics_context, Rndr::ShaderDesc{.type = Rndr::ShaderType::Fragment,
                                                                   .source = fragment_shader_contents,
                                                                   .debug_name = "Shape 2D Renderer - Fragment Shader"}};
    RNDR_ASSERT(m_fragment_shader.IsValid(), "Failed to initialize fragment shader!");

    const Rndr::InputLayoutDesc input_layout = Rndr::InputLayoutBuilder()
                                                   .AddVertexBuffer(m_vertex_buffer, 0, Rndr::DataRepetition::PerVertex)
                                                   .AddIndexBuffer(m_index_buffer)
                                                   .Build();

    m_pipeline = {m_desc.graphics_context, Rndr::PipelineDesc{.vertex_shader = &m_vertex_shader,
                                                              .pixel_shader = &m_fragment_shader,
                                                              .input_layout = input_layout,
                                                              .debug_name = "Shape 2D Renderer - Pipeline"}};
    RNDR_ASSERT(m_pipeline.IsValid(), "Failed to initialize pipeline!");
}

Rndr::Shape2DRenderer::~Shape2DRenderer()
{
    Destroy();
}

void Rndr::Shape2DRenderer::Destroy()
{
    m_per_frame_data_buffer.Destroy();
    m_vertex_buffer.Destroy();
    m_index_buffer.Destroy();
    m_vertex_shader.Destroy();
    m_fragment_shader.Destroy();
    m_pipeline.Destroy();
}

void Rndr::Shape2DRenderer::SetFrameBufferTarget(Opal::Ref<FrameBuffer> target)
{
    m_target = target;
    if (m_target.IsValid())
    {
        m_fb_width = m_target->GetWidth();
        m_fb_height = m_target->GetHeight();
    }
    else
    {
        m_fb_width = m_desc.swap_chain->GetDesc().width;
        m_fb_height = m_desc.swap_chain->GetDesc().height;
    }
}

bool Rndr::Shape2DRenderer::Render(f32 delta_seconds, Rndr::CommandList& cmd_list)
{
    RNDR_UNUSED(delta_seconds);

    Rndr::Matrix4x4f projection = Rndr::OrthographicOpenGL(0, static_cast<f32>(m_fb_width), 0, static_cast<f32>(m_fb_height), -1, 1);
    projection = Opal::Transpose(projection);

    if (m_target.IsValid())
    {
        cmd_list.CmdBindFrameBuffer(*m_target);
    }
    else
    {
        cmd_list.CmdBindSwapChainFrameBuffer(m_desc.swap_chain);
    }

    cmd_list.CmdUpdateBuffer(m_per_frame_data_buffer, Opal::AsBytes(projection));
    cmd_list.CmdUpdateBuffer(m_vertex_buffer, Opal::AsBytes(m_vertices));
    cmd_list.CmdUpdateBuffer(m_index_buffer, Opal::AsBytes(m_indices));

    cmd_list.CmdBindBuffer(m_per_frame_data_buffer, 0);
    cmd_list.CmdBindPipeline(m_pipeline);

    cmd_list.CmdDrawIndices(Rndr::PrimitiveTopology::Triangle, static_cast<i32>(m_indices.GetSize()));

    m_vertices.Clear();
    m_indices.Clear();

    return true;
}

void Rndr::Shape2DRenderer::DrawTriangle(const Rndr::Point2f& a, const Rndr::Point2f& b, const Rndr::Point2f& c,
                                         const Rndr::Vector4f& color)
{
    const i32 m_vertex_base = static_cast<i32>(m_vertices.GetSize());
    m_vertices.PushBack({.pos = a, .color = color});
    m_vertices.PushBack({.pos = b, .color = color});
    m_vertices.PushBack({.pos = c, .color = color});

    m_indices.PushBack(m_vertex_base + 0);
    m_indices.PushBack(m_vertex_base + 1);
    m_indices.PushBack(m_vertex_base + 2);
}

void Rndr::Shape2DRenderer::DrawRect(const Rndr::Point2f& bottom_left, const Rndr::Vector2f& size, const Rndr::Vector4f& color)
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

void Rndr::Shape2DRenderer::DrawLine(const Rndr::Point2f& start, const Rndr::Point2f& end, const Rndr::Vector4f& color, f32 thickness)
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

void Rndr::Shape2DRenderer::DrawArrow(const Rndr::Point2f& start, const Rndr::Vector2f& direction, const Rndr::Vector4f& color, f32 length,
                                      f32 body_thickness, f32 head_thickness, f32 body_to_head_ratio)
{
    RNDR_ASSERT(body_to_head_ratio > 0,
                "Body to head ratio needs to be larger then one! For example if its 3 then total length is divided in 4 parts and 3 are "
                "used for body.");
    const f32 body_length = body_to_head_ratio * length / (body_to_head_ratio + 1);
    const f32 head_length = length / (body_to_head_ratio + 1);

    const Rndr::Vector2f dir = Opal::Normalize(direction);
    const Rndr::Point2f body_end = start + body_length * dir;
    DrawLine(start, body_end, color, body_thickness);

    const Rndr::Vector2f perp(-dir.y, dir.x);
    DrawTriangle(body_end + (head_thickness / 2.0f) * perp, body_end - (head_thickness / 2.0f) * perp, body_end + head_length * dir, color);
}

void Rndr::Shape2DRenderer::DrawBezierSquare(const Rndr::Point2f& start, const Rndr::Point2f& control, const Rndr::Point2f& end,
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

void Rndr::Shape2DRenderer::DrawBezierCubic(const Rndr::Point2f& start, const Rndr::Point2f& control0, const Rndr::Point2f& control1,
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

void Rndr::Shape2DRenderer::DrawCircle(const Rndr::Point2f& center, f32 radius, const Rndr::Vector4f& color, i32 segment_count)
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