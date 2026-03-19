#include "rndr/canvas/shape-renderer.hpp"

#include "rndr/canvas/context.hpp"
#include "rndr/canvas/projections.hpp"

static const Opal::StringUtf8 k_shader_source = R"(
struct VertexInput
{
    float2 position : POSITION;
    float4 color : COLOR;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

uniform float4x4 mvp;

[shader("vertex")]
VertexOutput VertexMain(VertexInput in)
{
    VertexOutput out;
    out.position = mul(mvp, float4(in.position, 0, 1));
    out.color = in.color;
    return out;
}

[shader("fragment")]
float4 FragmentMain(VertexOutput in)
{
    return in.color;
}
)";

Rndr::Canvas::ShapeRenderer::ShapeRenderer(Opal::Ref<Context> context)
    : m_context(std::move(context))
{
    m_shader = Shader::FromSourceInMemory(k_shader_source);
    RNDR_ASSERT(m_shader.IsValid(), "Failed to create ShapeRenderer shader!");

    const VertexLayout vertex_layout = m_shader.GetVertexLayout().Clone();
    m_mesh = Mesh(vertex_layout, k_max_vertex_count, k_max_index_count, "ShapeRenderer Mesh");
    RNDR_ASSERT(m_mesh.IsValid(), "Failed to create ShapeRenderer mesh!");

    m_brush = Brush(BrushDesc{.cull_mode = CullMode::None});
    m_brush.SetShader(m_shader);
    RNDR_ASSERT(m_brush.IsValid(), "Failed to create ShapeRenderer brush!");
}

Rndr::Canvas::ShapeRenderer::~ShapeRenderer()
{
    Destroy();
}

void Rndr::Canvas::ShapeRenderer::Destroy()
{
    m_mesh.Destroy();
    m_shader.Destroy();
}

void Rndr::Canvas::ShapeRenderer::BeginFrame()
{
    m_mesh.Clear();
}

void Rndr::Canvas::ShapeRenderer::Render(DrawList& draw_list)
{
    const f32 width = static_cast<f32>(m_context->GetWidth());
    const f32 height = static_cast<f32>(m_context->GetHeight());
    const Matrix4x4f mvp = Orthographic(0, width, 0, height, -1.0f, 1.0f);

    m_brush.SetUniform("mvp", mvp);

    draw_list.Draw(m_mesh, m_brush);
}

void Rndr::Canvas::ShapeRenderer::DrawTriangle(const Point2f& a, const Point2f& b, const Point2f& c, const Vector4f& color)
{
    const u32 base = m_mesh.GetVertexCount();
    const VertexData vertices[3] = {
        {.pos = a, .color = color},
        {.pos = b, .color = color},
        {.pos = c, .color = color},
    };
    const u32 indices[3] = {base + 0, base + 1, base + 2};
    m_mesh.Append(Opal::AsBytes(vertices), Opal::AsBytes(indices));
}

void Rndr::Canvas::ShapeRenderer::DrawRect(const Point2f& bottom_left, const Vector2f& size, const Vector4f& color)
{
    const u32 base = m_mesh.GetVertexCount();
    const VertexData vertices[4] = {
        {.pos = bottom_left, .color = color},
        {.pos = bottom_left + Vector2f{size.x, 0}, .color = color},
        {.pos = bottom_left + Vector2f{size.x, size.y}, .color = color},
        {.pos = bottom_left + Vector2f{0, size.y}, .color = color},
    };
    const u32 indices[6] = {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3};
    m_mesh.Append(Opal::AsBytes(vertices), Opal::AsBytes(indices));
}

void Rndr::Canvas::ShapeRenderer::DrawLine(const Point2f& start, const Point2f& end, const Vector4f& color, f32 thickness)
{
    Vector2f dir = end - start;
    dir = Opal::Normalize(dir);
    const Vector2f perp(-dir.y, dir.x);

    const u32 base = m_mesh.GetVertexCount();
    const VertexData vertices[4] = {
        {.pos = start + 0.5f * thickness * perp, .color = color},
        {.pos = start - 0.5f * thickness * perp, .color = color},
        {.pos = end - 0.5f * thickness * perp, .color = color},
        {.pos = end + 0.5f * thickness * perp, .color = color},
    };
    const u32 indices[6] = {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3};
    m_mesh.Append(Opal::AsBytes(vertices), Opal::AsBytes(indices));
}

void Rndr::Canvas::ShapeRenderer::DrawArrow(const Point2f& start, const Vector2f& direction, const Vector4f& color, f32 length,
                                            f32 body_thickness, f32 head_thickness, f32 body_to_head_ratio)
{
    RNDR_ASSERT(body_to_head_ratio > 0,
                "Body to head ratio needs to be larger then one! For example if its 3 then total length is divided in 4 parts and 3 are "
                "used for body.");
    const f32 body_length = body_to_head_ratio * length / (body_to_head_ratio + 1);
    const f32 head_length = length / (body_to_head_ratio + 1);

    const Vector2f dir = Opal::Normalize(direction);
    const Point2f body_end = start + body_length * dir;
    DrawLine(start, body_end, color, body_thickness);

    const Vector2f perp(-dir.y, dir.x);
    DrawTriangle(body_end + (head_thickness / 2.0f) * perp, body_end - (head_thickness / 2.0f) * perp, body_end + head_length * dir, color);
}

void Rndr::Canvas::ShapeRenderer::DrawBezierSquare(const Point2f& start, const Point2f& control, const Point2f& end,
                                                   const Vector4f& color, f32 thickness, i32 segment_count)
{
    Point2f curr_start = start;
    for (i32 segment_idx = 0; segment_idx < segment_count; ++segment_idx)
    {
        const f32 t = static_cast<f32>(segment_idx + 1) * (1.0f / static_cast<f32>(segment_count));
        Point2f curr_end = Point2f::Zero();
        curr_end.x = (1 - t) * (1 - t) * start.x + 2 * (1 - t) * t * control.x + t * t * end.x;
        curr_end.y = (1 - t) * (1 - t) * start.y + 2 * (1 - t) * t * control.y + t * t * end.y;
        DrawLine(curr_start, curr_end, color, thickness);
        curr_start = curr_end;
    }
}

void Rndr::Canvas::ShapeRenderer::DrawBezierCubic(const Point2f& start, const Point2f& control0, const Point2f& control1,
                                                  const Point2f& end, const Vector4f& color, f32 thickness, i32 segment_count)
{
    Point2f curr_start = start;
    for (i32 segment_idx = 0; segment_idx < segment_count; ++segment_idx)
    {
        const f32 t = static_cast<f32>(segment_idx + 1) * (1.0f / static_cast<f32>(segment_count));
        Point2f curr_end = Point2f::Zero();
        curr_end.x = (1 - t) * (1 - t) * (1 - t) * start.x + 3 * (1 - t) * (1 - t) * t * control0.x + 3 * (1 - t) * t * t * control1.x +
                     t * t * t * end.x;
        curr_end.y = (1 - t) * (1 - t) * (1 - t) * start.y + 3 * (1 - t) * (1 - t) * t * control0.y + 3 * (1 - t) * t * t * control1.y +
                     t * t * t * end.y;
        DrawLine(curr_start, curr_end, color, thickness);
        curr_start = curr_end;
    }
}

void Rndr::Canvas::ShapeRenderer::DrawCircle(const Point2f& center, f32 radius, const Vector4f& color, i32 segment_count)
{
    Point2f curr_start = center + Vector2f{radius, 0};
    for (i32 segment_idx = 0; segment_idx < segment_count; ++segment_idx)
    {
        const f32 step = 360.0f / static_cast<f32>(segment_count);
        const f32 curr_step_radians = Opal::Radians(static_cast<f32>(segment_idx + 1) * step);
        Point2f curr_end{radius * Opal::Cos(curr_step_radians), radius * Opal::Sin(curr_step_radians)};
        curr_end += Vector2f{center.x, center.y};
        DrawTriangle(curr_start, curr_end, center, color);
        curr_start = curr_end;
    }
}
