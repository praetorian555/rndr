#include "rndr/canvas/cubemap-renderer.hpp"

#include "rndr/canvas/context.hpp"

static const Opal::StringUtf8 k_shader_source = R"(
struct VertexInput
{
    float2 position : POSITION;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD;
};

uniform float4x4 inverse_vp;

[shader("vertex")]
VertexOutput VertexMain(VertexInput in)
{
    VertexOutput out;
    out.position = float4(in.position, 0.999, 1);
    float4 world_pos = mul(inverse_vp, float4(in.position, 1, 1));
    out.direction = world_pos.xyz / world_pos.w;
    return out;
}

uniform SamplerCube cubemap;

[shader("fragment")]
float4 FragmentMain(VertexOutput in)
{
    return cubemap.Sample(normalize(in.direction));
}
)";

Rndr::Canvas::CubemapRenderer::CubemapRenderer(Opal::Ref<Context> context)
    : m_context(std::move(context))
{
    m_shader = Shader::FromSourceInMemory(k_shader_source, "Cube Map Renderer");
    RNDR_ASSERT(m_shader.IsValid(), "Failed to create CubemapRenderer shader!");

    const VertexLayout vertex_layout = m_shader.GetVertexLayout().Clone();

    // Full-screen triangle in NDC. Vertex positions extend beyond [-1,1] so that a single
    // triangle covers the entire screen after clipping.
    struct Vertex
    {
        Point2f pos;
    };
    const Vertex vertices[3] = {
        {.pos = {-1, -1}},
        {.pos = { 3, -1}},
        {.pos = {-1,  3}},
    };
    const u32 indices[3] = {0, 1, 2};
    m_mesh = Mesh(vertex_layout, Opal::AsBytes(vertices), Opal::AsBytes(indices), "CubemapRenderer Mesh");
    RNDR_ASSERT(m_mesh.IsValid(), "Failed to create CubemapRenderer mesh!");

    m_brush = Brush(BrushDesc{.depth_test = false, .depth_write = false, .cull_mode = CullMode::None});
    m_brush.SetShader(m_shader);
    RNDR_ASSERT(m_brush.IsValid(), "Failed to create CubemapRenderer brush!");
}

Rndr::Canvas::CubemapRenderer::~CubemapRenderer()
{
    Destroy();
}

void Rndr::Canvas::CubemapRenderer::Destroy()
{
    m_mesh.Destroy();
    m_shader.Destroy();
}

void Rndr::Canvas::CubemapRenderer::SetCubemap(const Texture& cubemap)
{
    m_brush.SetTexture("cubemap", cubemap);
}

void Rndr::Canvas::CubemapRenderer::Render(DrawList& draw_list, const Matrix4x4f& inverse_vp)
{
    m_brush.SetUniform("inverse_vp", inverse_vp);
    draw_list.Draw(m_mesh, m_brush);
}
