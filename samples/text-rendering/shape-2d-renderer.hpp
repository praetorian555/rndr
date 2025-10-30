#pragma once

#include "opal/container/ref.h"

#include "rndr/math.hpp"

#include "rndr/render-api.hpp"
#include "types.hpp"

namespace Rndr
{
class CommandList;
class GraphicsContext;
}

class Shape2DRenderer
{
public:
    bool Init(Rndr::GraphicsContext* gc, i32 fb_width, i32 fb_height);
    void Destroy();

    void SetFrameBufferSize(i32 width, i32 height);

    void Render(f32 delta_seconds, Rndr::CommandList& cmd_list);

    bool DrawRect(const Rndr::Point2f& bottom_left, const Rndr::Vector2f& size, const Rndr::Vector4f& color);
    bool DrawCircle(const Rndr::Point2f& center, f32 radius, const Rndr::Vector4f& color, i32 triangle_count = 8);
    bool DrawLine(const Rndr::Point2f& start, Rndr::Point2f end, const Rndr::Vector4f& color, f32 thickness = 2);

private:
    constexpr static i32 k_max_vertex_count = 1024;

    struct VertexData
    {
        Rndr::Point2f pos;
        Rndr::Vector4f color;
    };

    i32 m_fb_width = 0;
    i32 m_fb_height = 0;

    Opal::Ref<Rndr::GraphicsContext> m_gc;
    Rndr::Buffer m_per_frame_data_buffer;
    Rndr::Buffer m_vertex_buffer;
    Rndr::Buffer m_index_buffer;
    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_fragment_shader;
    Rndr::Pipeline m_pipeline;

    Opal::DynamicArray<VertexData> m_vertices;
    Opal::DynamicArray<i32> m_indices;
};