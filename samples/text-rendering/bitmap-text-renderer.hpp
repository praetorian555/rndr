#pragma once

#include "stb_truetype/stb_truetype.h"

#include "opal/clonable-base.h"

#include "rndr/canvas/brush.hpp"
#include "rndr/math.hpp"
#include "rndr/canvas/mesh.hpp"
#include "rndr/canvas/shader.hpp"
#include "rndr/canvas/texture.hpp"

#include "types.hpp"

namespace Rndr::Canvas
{
class Context;
}  // namespace Rndr::Canvas

struct BitmapTextRendererDesc : Opal::ClonableBase<BitmapTextRendererDesc>
{
    Opal::StringUtf8 font_file_path;
    f32 font_size = 64.0f;
    i32 first_code_point = 32;  // ASCII
    i32 code_point_count = 95;
    i32 max_char_render_count = 1024;
    u32 oversample_h = 0;  // If left as zero it will be equal to 2 if font_size is less then 36 or 1 otherwise
    u32 oversample_v = 1;
    f32 alpha_multiplier = 1.0f;

    OPAL_CLONE_FIELDS(font_file_path, font_size, first_code_point, code_point_count, max_char_render_count, oversample_h, oversample_v,
                      alpha_multiplier);
};

class BitmapTextRenderer
{
public:
    bool Init(Opal::Ref<Rndr::Canvas::Context> context, const BitmapTextRendererDesc& desc);
    void Destroy();
    void UpdateFontSize(f32 font_size);
    void UpdateFontOversampling(u32 oversample_h, u32 oversample_v);
    void SetAlphaMultiplier(f32 alpha_multiplier);

    bool DrawText(const Opal::StringUtf8& text, const Rndr::Vector2f& position, const Rndr::Vector4f& color);

    void Render(f32 delta_seconds);

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

    Opal::Ref<Rndr::Canvas::Context> m_context;
    Rndr::Canvas::Shader m_shader;
    Rndr::Canvas::Brush m_brush;
    Rndr::Canvas::Mesh m_mesh;
    Rndr::Canvas::Texture m_glyph_atlas;

    BitmapTextRendererDesc m_desc;
    Opal::DynamicArray<Rndr::u8> m_font_contents;
    stbtt_fontinfo m_font_info = {};
    Opal::DynamicArray<u8> m_atlas_data;
    Opal::DynamicArray<stbtt_packedchar> m_packed_chars;
    Opal::DynamicArray<stbtt_aligned_quad> m_aligned_quads;
    Opal::DynamicArray<VertexData> m_vertices;
    Opal::DynamicArray<i32> m_indices;
};
