#include "rndr/canvas/renderers/grid-renderer.hpp"

#include "rndr/canvas/context.hpp"
#include "rndr/trace.hpp"

static const Opal::StringUtf8 k_shader_source = R"(
struct VertexInput
{
    float3 position : POSITION;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 world_position : TEXCOORD0;
};

cbuffer FrameConstants
{
    uniform float4x4 view;
    uniform float4x4 projection;
};

[shader("vertex")]
VertexOutput VertexMain(VertexInput in)
{
    VertexOutput out;
    out.position = mul(projection, mul(view, float4(in.position, 1)));
    out.world_position = in.position;
    return out;
}

[shader("fragment")]
float4 FragmentMain(VertexOutput in)
{
    float2 uv = in.world_position.xz;
    float2 deriv_uv = fwidth(uv);
    float2 line_aa = 1.5 * deriv_uv;
    float line_width = 1.0 / 50.0;
    float2 draw_width = clamp(float2(line_width, line_width), deriv_uv, float2(0.5, 0.5));

    // Triangle function that is 0 at whole numbers and grows to 1 at the 0.5 coordinates.
    float2 grid_uv = 1.0 - abs(frac(uv) * 2.0 - 1.0);
    float2 grid_2 = 1.0 - smoothstep(draw_width - line_aa, draw_width + line_aa, grid_uv);
    grid_2 *= clamp(float2(line_width, line_width) / draw_width, 0.0, 1.0);

    // Two transparent lines overlapping, alpha blended.
    float grid = lerp(grid_2.x, 1.0, grid_2.y);

    float4 out_color = float4(0, 0, 0, 1);
    if (uv.x > -draw_width.x && uv.x < draw_width.x)
    {
        // Z-axis in blue.
        out_color.z = 1.0;
    }
    else if (uv.y > -draw_width.y && uv.y < draw_width.y)
    {
        // X-axis in red.
        out_color.x = 1.0;
    }
    else
    {
        out_color.xyz = float3(grid, grid, grid);
        if (grid < 0.02)
        {
            out_color.a = 0.0;
        }
    }

    return out_color;
}
)";

Rndr::Canvas::GridRenderer::GridRenderer(Opal::Ref<Context> context)
    : m_context(std::move(context))
{
    m_shader = Shader::FromSourceInMemory(k_shader_source, "Grid Renderer Shader");
    RNDR_ASSERT(m_shader.IsValid(), "Failed to create GridRenderer shader!");

    const VertexLayout vertex_layout = m_shader.GetVertexLayout().Clone();

    // Large ground-plane quad on the XZ plane (two triangles).
    struct Vertex
    {
        Point3f pos;
    };
    constexpr f32 k_extent = 1000.0f;
    constexpr Vertex k_vertices[4] = {
        {.pos = {-k_extent, 0, -k_extent}},
        {.pos = { k_extent, 0, -k_extent}},
        {.pos = { k_extent, 0,  k_extent}},
        {.pos = {-k_extent, 0,  k_extent}},
    };
    const u32 indices[6] = {2, 0, 3, 0, 2, 1};
    m_mesh = Mesh(vertex_layout, Opal::AsBytes(k_vertices), Opal::AsBytes(indices), "GridRenderer Mesh");
    RNDR_ASSERT(m_mesh.IsValid(), "Failed to create GridRenderer mesh!");

    m_brush = Brush(BrushDesc{.blend_mode = BlendMode::Alpha, .depth_test = true, .cull_mode = CullMode::None}, "Grid Renderer Brush");
    m_brush.SetShader(m_shader);
    RNDR_ASSERT(m_brush.IsValid(), "Failed to create GridRenderer brush!");
}

Rndr::Canvas::GridRenderer::~GridRenderer()
{
    Destroy();
}

void Rndr::Canvas::GridRenderer::Destroy()
{
    m_mesh.Destroy();
    m_shader.Destroy();
}

void Rndr::Canvas::GridRenderer::Render(DrawList& draw_list, const Matrix4x4f& view, const Matrix4x4f& projection)
{
    m_brush.SetUniform("view", view);
    m_brush.SetUniform("projection", projection);
    draw_list.Draw(m_mesh, m_brush);
}
