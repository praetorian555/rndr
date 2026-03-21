#pragma once

#include "opal/container/ref.h"

#include "rndr/canvas/brush.hpp"
#include "rndr/canvas/draw-list.hpp"
#include "rndr/canvas/mesh.hpp"
#include "rndr/canvas/shader.hpp"
#include "rndr/math.hpp"
#include "rndr/types.hpp"

namespace Rndr::Canvas
{

class Context;

class ShapeRenderer
{
public:
    explicit ShapeRenderer(Opal::Ref<Context> context);
    ~ShapeRenderer();

    ShapeRenderer(const ShapeRenderer&) = delete;
    ShapeRenderer& operator=(const ShapeRenderer&) = delete;
    ShapeRenderer(ShapeRenderer&&) noexcept = default;
    ShapeRenderer& operator=(ShapeRenderer&&) noexcept = default;

    void Destroy();

    void BeginFrame();
    void Render(DrawList& draw_list);

    void DrawTriangle(const Point2f& a, const Point2f& b, const Point2f& c, const Vector4f& color);
    void DrawRect(const Point2f& bottom_left, const Vector2f& size, const Vector4f& color);
    void DrawLine(const Point2f& start, const Point2f& end, const Vector4f& color, f32 thickness = 2);
    void DrawArrow(const Point2f& start, const Vector2f& direction, const Vector4f& color, f32 length, f32 body_thickness = 2,
                   f32 head_thickness = 4, f32 body_to_head_ratio = 3);
    void DrawBezierSquare(const Point2f& start, const Point2f& control, const Point2f& end, const Vector4f& color, f32 thickness = 2,
                          i32 segment_count = 8);
    void DrawBezierCubic(const Point2f& start, const Point2f& control0, const Point2f& control1, const Point2f& end,
                         const Vector4f& color, f32 thickness = 2, i32 segment_count = 8);
    void DrawCircle(const Point2f& center, f32 radius, const Vector4f& color, i32 segment_count = 16);

private:
    constexpr static i32 k_max_vertex_count = 100 * 1024;
    constexpr static i32 k_max_index_count = 2 * k_max_vertex_count;

    struct VertexData
    {
        Point2f pos;
        Vector4f color;
    };

    Opal::Ref<Context> m_context;
    Shader m_shader;
    Brush m_brush;
    Mesh m_mesh;
};

}  // namespace Rndr::Canvas
