#include "rndr/renderers/grid-renderer.hpp"

#include "opal/paths.h"
#include "opal/time.h"

OPAL_START_DISABLE_WARNINGS
OPAL_DISABLE_MSVC_WARNING(4324)
struct RNDR_ALIGN(16) Uniforms
{
    Rndr::Matrix4x4f view;
    Rndr::Matrix4x4f projection;
    Rndr::Vector3f position;
};
OPAL_END_DISABLE_WARNINGS

Rndr::GridRenderer::GridRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc, Opal::Ref<FrameBuffer> target)
    : RendererBase(name, desc), m_target(target)
{
    const BufferDesc buffer_desc{.usage = Rndr::Usage::Dynamic, .size = sizeof(Uniforms), .stride = sizeof(Uniforms)};
    m_uniform_buffer = Buffer(m_desc.graphics_context, buffer_desc);
    RNDR_ASSERT(m_uniform_buffer.IsValid(), "Invalid buffer!");

    const Opal::StringUtf8 vertex_shader_contents = R"(
        #version 460

        layout(set = 0, binding = 0) uniform ViewUniforms {
            mat4 view;
            mat4 proj;
            vec3 pos;
        } view;

        vec3 grid_plane[6] = vec3[](
            vec3(1000, 0, 1000), vec3(-1000, 0, -1000), vec3(-100, 0, 1000),
            vec3(-1000, 0, -1000), vec3(1000, 0, 1000), vec3(100, 0, -1000)
        );

        layout(location=0) out vec4 world_position;

        void main()
        {
            gl_Position = view.proj * view.view * vec4(grid_plane[gl_VertexID].xyz, 1);
            world_position = vec4(grid_plane[gl_VertexID].xyz, 1);
        }
    )";

    const Opal::StringUtf8 fragment_shader_contents = R"(
        #version 460

        layout(location = 0) in vec4 world_position;

        layout(location = 0) out vec4 out_color;

        void main()
        {
            vec2 uv = world_position.xz;
            vec2 deriv_uv = fwidth(uv);
            vec2 line_aa = 1.5 * deriv_uv;
            float line_width = 1 / 50.0f;
            vec2 draw_width = clamp(vec2(line_width), deriv_uv, vec2(0.5));
            // Triangle function that is 0 at whole numbers and grows to 1 an the .5 coordinates
            vec2 grid_uv = 1.0 - abs(fract(uv) * 2.0 - 1.0);
            // I don't quite understand this
            vec2 grid_2 = 1.0f - smoothstep(draw_width - line_aa, draw_width + line_aa, grid_uv);
            grid_2 *= clamp(vec2(line_width, line_width) / draw_width, 0.0f, 1.0f);
            // Since we have two transparent lines overlapping, we are doing alpha blending essentially
            float grid = mix(grid_2.x, 1.0f, grid_2.y);

            out_color = vec4(0, 0, 0, 1.0f);
            if (uv.x > -draw_width.x && uv.x < draw_width.x)
            {
                out_color.z = 1.0f;
            }
            else if (uv.y > -draw_width.y && uv.y < draw_width.y)
            {
                out_color.x = 1.0f;
            }
            else
            {
                out_color.xyz = vec3(grid);
            }
        }
    )";

    const ShaderDesc vertex_shader_desc{.type = ShaderType::Vertex, .source = vertex_shader_contents};
    m_vertex_shader = Shader(m_desc.graphics_context, vertex_shader_desc);
    RNDR_ASSERT(m_vertex_shader.IsValid(), "Invalid vertex shader!");

    const ShaderDesc fragment_shader_desc{.type = ShaderType::Fragment, .source = fragment_shader_contents};
    m_fragment_shader = Shader(m_desc.graphics_context, fragment_shader_desc);
    RNDR_ASSERT(m_fragment_shader.IsValid(), "Invalid fragment shader!");

    const PipelineDesc pipeline_desc{.vertex_shader = &m_vertex_shader, .pixel_shader = &m_fragment_shader};
    m_pipeline = Pipeline(m_desc.graphics_context, pipeline_desc);
    RNDR_ASSERT(m_pipeline.IsValid(), "Invalid pipeline!");
}

Rndr::GridRenderer::~GridRenderer()
{
    Destroy();
}

void Rndr::GridRenderer::Destroy()
{
    m_uniform_buffer.Destroy();
    m_pipeline.Destroy();
    m_vertex_shader.Destroy();
    m_fragment_shader.Destroy();
}

void Rndr::GridRenderer::SetFrameBufferTarget(Opal::Ref<FrameBuffer> target)
{
    m_target = target;
}

void Rndr::GridRenderer::SetTransforms(const Matrix4x4f& view, const Matrix4x4f& projection)
{
    m_view = view;
    m_projection = projection;
}

bool Rndr::GridRenderer::Render(f32 delta_seconds, CommandList& command_list)
{
    RNDR_UNUSED(delta_seconds);
    Uniforms uniforms;
    uniforms.view = Opal::Transpose(m_view);
    uniforms.projection = Opal::Transpose(m_projection);
    command_list.CmdBindBuffer(m_uniform_buffer, 0);
    command_list.CmdUpdateBuffer(m_uniform_buffer, Opal::AsBytes(uniforms));
    command_list.CmdBindPipeline(m_pipeline);
    if (m_target.IsValid())
    {
        command_list.CmdBindFrameBuffer(m_target);
    }
    else
    {
        command_list.CmdBindSwapChainFrameBuffer(m_desc.swap_chain);
    }
    command_list.CmdDrawVertices(PrimitiveTopology::Triangle, 6);
    return true;
}
