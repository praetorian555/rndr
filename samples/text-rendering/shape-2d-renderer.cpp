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

    const Rndr::InputLayoutDesc input_layout =
        Rndr::InputLayoutBuilder().AddVertexBuffer(m_vertex_buffer, 0, Rndr::DataRepetition::PerVertex).AddIndexBuffer(m_index_buffer).Build();

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

bool Shape2DRenderer::DrawRect(const Rndr::Point2f& bottom_left, const Rndr::Vector2f& size, const Rndr::Vector4f& color)
{
    m_vertices.PushBack({.pos = bottom_left, .color = color});
    m_vertices.PushBack({.pos = bottom_left + Rndr::Vector2f{size.x, 0}, .color = color});
    m_vertices.PushBack({.pos = bottom_left + Rndr::Vector2f{size.x, size.y}, .color = color});
    m_vertices.PushBack({.pos = bottom_left + Rndr::Vector2f{0, size.y}, .color = color});

    m_indices.PushBack(0);
    m_indices.PushBack(1);
    m_indices.PushBack(2);
    m_indices.PushBack(0);
    m_indices.PushBack(2);
    m_indices.PushBack(3);

    return true;
}