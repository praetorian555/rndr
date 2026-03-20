#include "rndr/canvas/bitmap-text-renderer.hpp"

#include "opal/paths.h"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/projections.hpp"
#include "rndr/file.hpp"
#include "rndr/log.hpp"

bool Rndr::Canvas::BitmapTextRenderer::Init(Opal::Ref<Context> context, const BitmapTextRendererDesc& desc)
{
    m_context = std::move(context);
    m_desc = desc.Clone();
    if (m_desc.oversample_h == 0)
    {
        m_desc.oversample_h = m_desc.font_size < 36.0f ? 2 : 1;
    }

    UpdateFontAtlas();

    const Opal::StringUtf8 shader_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "shaders", "bitmap-text-render.slang");
    m_shader = Shader::FromSource(shader_path, "Bitmap Text Renderer");
    RNDR_ASSERT(m_shader.IsValid(), "Shader could not be created!");

    const VertexLayout vertex_layout = m_shader.GetVertexLayout().Clone();
    m_mesh = Mesh(vertex_layout, m_desc.max_char_render_count * k_char_vertex_count,
                  m_desc.max_char_render_count * k_char_index_count, "Bitmap Text Renderer Mesh");
    RNDR_ASSERT(m_mesh.IsValid(), "Mesh could not be created!");

    m_brush = Brush(BrushDesc{});
    m_brush.SetShader(m_shader);
    m_brush.SetBlendMode(BlendMode::Alpha);
    RNDR_ASSERT(m_brush.IsValid(), "Failed to create a brush!");

    return true;
}

void Rndr::Canvas::BitmapTextRenderer::UpdateFontAtlas()
{
    if (m_font_contents.IsEmpty())
    {
        m_font_contents = Rndr::File::ReadEntireFile(m_desc.font_file_path);
        RNDR_ASSERT(!m_font_contents.IsEmpty(), "Invalid font path");
        const i32 font_count = stbtt_GetNumberOfFonts(m_font_contents.GetData());
        if (font_count == 0)
        {
            throw Opal::Exception("No fonts in the font file!");
        }
        m_packed_chars.Resize(m_desc.code_point_count);
        m_aligned_quads.Resize(m_desc.code_point_count);
        stbtt_InitFont(&m_font_info, m_font_contents.GetData(), stbtt_GetFontOffsetForIndex(m_font_contents.GetData(), 0));
    }

    m_atlas_data = Opal::DynamicArray<u8>(k_atlas_height * k_atlas_width);

    // Render font into the atlas
    stbtt_pack_context pack_context{};
    stbtt_PackBegin(&pack_context, m_atlas_data.GetData(), k_atlas_width, k_atlas_height, k_atlas_width, 1, nullptr);
    stbtt_PackSetOversampling(&pack_context, m_desc.oversample_h, m_desc.oversample_v);
    stbtt_PackFontRange(&pack_context, m_font_contents.GetData(), 0, m_desc.font_size, m_desc.first_code_point, m_desc.code_point_count,
                        m_packed_chars.GetData());
    for (i32 code_point_idx = 0; code_point_idx < m_desc.code_point_count; ++code_point_idx)
    {
        f32 x = 0;
        f32 y = 0;
        stbtt_GetPackedQuad(m_packed_chars.GetData(), k_atlas_width, k_atlas_height, code_point_idx, &x, &y,
                            &m_aligned_quads[code_point_idx], 0);
    }
    for (u8& pixel : m_atlas_data)
    {
        u32 val = pixel;
        val = static_cast<u32>(static_cast<f32>(val) * m_desc.alpha_multiplier);
        pixel = val > 255 ? 255 : val & 0x000000ff;
    }

    // Useful for debugging to dump rasterized atlas
    const Rndr::Bitmap bitmap(k_atlas_width, k_atlas_height, 1, Rndr::PixelFormat::R8_UNORM, 1, Opal::AsWritableBytes(m_atlas_data));
    Rndr::File::SaveImage(bitmap, "atlas.png");

    const TextureDesc k_texture_desc{
        .width = k_atlas_width, .height = k_atlas_height, .type = TextureType::Texture2D, .format = Format::R8};
    m_glyph_atlas = Texture(m_context, k_texture_desc, Opal::AsBytes(m_atlas_data), "Glyph Atlas");
    RNDR_ASSERT(m_glyph_atlas.IsValid(), "Glyph atlas could not be created!");
}

void Rndr::Canvas::BitmapTextRenderer::Destroy()
{
    m_glyph_atlas.Destroy();
    m_mesh.Destroy();
    m_shader.Destroy();
}

void Rndr::Canvas::BitmapTextRenderer::UpdateFontSize(f32 font_size)
{
    if (font_size != m_desc.font_size)
    {
        m_desc.font_size = font_size;
        UpdateFontAtlas();
    }
}

void Rndr::Canvas::BitmapTextRenderer::UpdateFontOversampling(u32 oversample_h, u32 oversample_v)
{
    if (oversample_h == 0)
    {
        oversample_h = m_desc.font_size < 36 ? 2 : 1;
    }

    if (m_desc.oversample_h != oversample_h || m_desc.oversample_v != oversample_v)
    {
        m_desc.oversample_h = oversample_h;
        m_desc.oversample_v = oversample_v;
        UpdateFontAtlas();
    }
}

void Rndr::Canvas::BitmapTextRenderer::SetAlphaMultiplier(f32 alpha_multiplier)
{
    if (m_desc.alpha_multiplier != alpha_multiplier)
    {
        m_desc.alpha_multiplier = alpha_multiplier;
        UpdateFontAtlas();
    }
}

bool Rndr::Canvas::BitmapTextRenderer::DrawText(const Opal::StringUtf8& text, const Vector2f& in_position, const Vector4f& color)
{
    Point2f curr_position = {in_position.x, in_position.y};
    curr_position = Opal::Floor(curr_position);
    char next_c = 0;
    for (i32 i = 0; i < text.GetSize(); i++)
    {
        const char c = text[i];
        if (i < text.GetSize() - 1)
        {
            next_c = text[i + 1];
        }
        const stbtt_packedchar* packed_char = &m_packed_chars[c - m_desc.first_code_point];
        const stbtt_aligned_quad* aligned_quad = &m_aligned_quads[c - m_desc.first_code_point];
        Vector2f glyph_size;
        glyph_size.x = static_cast<f32>(aligned_quad->x1 - aligned_quad->x0);
        glyph_size.y = static_cast<f32>(aligned_quad->y1 - aligned_quad->y0);

        Point2f glyph_bottom_left;
        // Discard position remainder
        glyph_bottom_left.x = Opal::Floor(curr_position.x + packed_char->xoff);
        glyph_bottom_left.y = Opal::Floor(curr_position.y - (packed_char->yoff + glyph_size.y));

        const VertexData glyph_vertices[4] = {
            {.pos = {glyph_bottom_left.x, glyph_bottom_left.y}, .uv = {aligned_quad->s0, aligned_quad->t1}, .color = color},
            {.pos = {glyph_bottom_left.x + glyph_size.x, glyph_bottom_left.y}, .uv = {aligned_quad->s1, aligned_quad->t1}, .color = color},
            {.pos = {glyph_bottom_left.x + glyph_size.x, glyph_bottom_left.y + glyph_size.y},
             .uv = {aligned_quad->s1, aligned_quad->t0},
             .color = color},
            {.pos = {glyph_bottom_left.x, glyph_bottom_left.y + glyph_size.y}, .uv = {aligned_quad->s0, aligned_quad->t0}, .color = color}};
        const u32 first_vertex_idx = m_mesh.GetVertexCount();
        const u32 glyph_indices[6] = {first_vertex_idx + 0, first_vertex_idx + 2, first_vertex_idx + 3,
                                      first_vertex_idx + 0, first_vertex_idx + 1, first_vertex_idx + 2};
        m_mesh.Append(Opal::AsBytes(glyph_vertices), Opal::AsBytes(glyph_indices));

        const f32 scale = stbtt_ScaleForPixelHeight(&m_font_info, m_desc.font_size);
        i32 kern = 0;
        if (next_c != 0)
        {
            kern = stbtt_GetCodepointKernAdvance(&m_font_info, c, next_c);
        }
        const f32 kern_scaled = static_cast<f32>(kern) * scale;

        // Snap the position to the closest integer
        curr_position.x += Opal::Round(packed_char->xadvance + kern_scaled);
    }

    return true;
}

void Rndr::Canvas::BitmapTextRenderer::BeginFrame()
{
    m_mesh.Clear();
}

void Rndr::Canvas::BitmapTextRenderer::Render(DrawList& draw_list)
{
    const f32 width = static_cast<f32>(m_context->GetWidth());
    const f32 height = static_cast<f32>(m_context->GetHeight());
    const Matrix4x4f mvp = Orthographic(0, width, 0, height, -1.0f, 1.0f);

    m_brush.SetUniform("mvp", mvp);
    m_brush.SetTexture("glyph_atlas", m_glyph_atlas);

    draw_list.Draw(m_mesh, m_brush);
}
