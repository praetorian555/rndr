#pragma once

#include "opal/container/ref.h"

#include "rndr/delegate.hpp"
#include "rndr/math.hpp"
#include "rndr/render-api.hpp"
#include "rndr/renderers/renderer-base.hpp"
#include "rndr/types.hpp"

namespace Rndr
{
class CommandList;
class GraphicsContext;

class Shape2DRenderer : public RendererBase
{
public:
    Shape2DRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc, Opal::Ref<FrameBuffer> target);
    ~Shape2DRenderer() override;

    void Destroy();

    void SetFrameBufferTarget(Opal::Ref<FrameBuffer> target);

    bool Render(f32 delta_seconds, CommandList& cmd_list) override;

    void DrawTriangle(const Rndr::Point2f& a, const Rndr::Point2f& b, const Rndr::Point2f& c, const Rndr::Vector4f& color);
    void DrawRect(const Rndr::Point2f& bottom_left, const Rndr::Vector2f& size, const Rndr::Vector4f& color);
    void DrawLine(const Rndr::Point2f& start, const Rndr::Point2f& end, const Rndr::Vector4f& color, f32 thickness = 2);
    void DrawArrow(const Rndr::Point2f& start, const Rndr::Vector2f& direction, const Rndr::Vector4f& color, f32 length,
                   f32 body_thickness = 2, f32 head_thickness = 4, f32 body_to_head_ratio = 3);
    void DrawBezierSquare(const Rndr::Point2f& start, const Rndr::Point2f& control, const Rndr::Point2f& end, const Rndr::Vector4f& color,
                          f32 thickness = 2, i32 segment_count = 8);
    void DrawBezierCubic(const Rndr::Point2f& start, const Rndr::Point2f& control0, const Rndr::Point2f& control1, const Rndr::Point2f& end,
                         const Rndr::Vector4f& color, f32 thickness = 2, i32 segment_count = 8);
    void DrawCircle(const Rndr::Point2f& center, f32 radius, const Rndr::Vector4f& color, i32 segment_count = 16);

private:
    constexpr static i32 k_max_vertex_count = 100 * 1024;

    struct VertexData
    {
        Point2f pos;
        Vector4f color;
    };

    i32 m_fb_width = 0;
    i32 m_fb_height = 0;

    Buffer m_per_frame_data_buffer;
    Buffer m_vertex_buffer;
    Buffer m_index_buffer;
    Shader m_vertex_shader;
    Shader m_fragment_shader;
    Pipeline m_pipeline;
    Opal::Ref<FrameBuffer> m_target;

    Opal::DynamicArray<VertexData> m_vertices;
    Opal::DynamicArray<i32> m_indices;
    DelegateHandle m_swap_chain_resize_handle;
};

}  // namespace Rndr
