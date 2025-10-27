#pragma once

#include "stb_truetype/stb_truetype.h"

#include "rndr/render-api.hpp"

#include "types.hpp"

struct BitmapTextRendererDesc
{
    Opal::StringUtf8 font_file_path;
    f32 font_size = 64.0f;
    i32 first_code_point = 32;  // ASCII
    i32 code_point_count = 95;
    i32 max_char_render_count = 1024;
    u32 oversample_h = 1;
    u32 oversample_v = 1;
};

class BitmapTextRenderer
{
public:
    bool Init(Rndr::GraphicsContext* gc, Rndr::FrameBuffer* frame_buffer, const BitmapTextRendererDesc& desc);
    void Destroy();

    void UpdateFrameBuffer(Rndr::FrameBuffer& fb);
    void UpdateFontSize(f32 font_size);
    void UpdateFontOversampling(u32 oversample_h, u32 oversample_v);

    bool DrawText(const Opal::StringUtf8& text, const Rndr::Vector2f& position, const Rndr::Vector4f& color);

    void Render(f32 delta_seconds, Rndr::CommandList& cmd_list);

private:
    void UpdateFontAtlas();

    struct RNDR_ALIGN(16) PerFrameData
    {
        Rndr::Matrix4x4f mvp;
    };

    struct RNDR_ALIGN(16) VertexData
    {
        Rndr::Point2f pos;
        Rndr::Point2f uv;
        Rndr::Vector4f color;
    };

    constexpr static i32 k_atlas_width = 1024;
    constexpr static i32 k_atlas_height = 1024;
    constexpr static i32 k_char_vertex_count = 4;
    constexpr static i32 k_char_index_count = 6;

    Rndr::GraphicsContext* m_gc = nullptr;
    Rndr::FrameBuffer* m_frame_buffer = nullptr;
    BitmapTextRendererDesc m_desc;
    Opal::DynamicArray<Rndr::u8> m_font_contents;
    stbtt_fontinfo m_font_info = {};
    Rndr::Texture m_atlas_texture;
    Rndr::Buffer m_vertex_buffer;
    Rndr::Buffer m_index_buffer;
    Rndr::Buffer m_per_frame_data_buffer;
    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_fragment_shader;
    Rndr::Pipeline m_pipeline;
    Opal::DynamicArray<stbtt_packedchar> m_packed_chars;
    Opal::DynamicArray<stbtt_aligned_quad> m_aligned_quads;
    Opal::DynamicArray<VertexData> m_vertices;
    Opal::DynamicArray<i32> m_indices;
};
