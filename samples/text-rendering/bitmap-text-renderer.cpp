#include "bitmap-text-renderer.hpp"

#include "rndr/file.hpp"
#include "rndr/input-layout-builder.hpp"
#include "rndr/log.hpp"
#include "rndr/projections.hpp"

bool BitmapTextRenderer::Init(Rndr::GraphicsContext* gc, Rndr::FrameBuffer* frame_buffer, const BitmapTextRendererDesc& desc)
{
    m_gc = gc;
    RNDR_ASSERT(m_gc != nullptr, "Invalid graphics context");
    m_frame_buffer = frame_buffer;
    RNDR_ASSERT(m_frame_buffer != nullptr, "Invalid frame buffer");
    m_desc = desc;

    UpdateFontAtlas();

    constexpr Rndr::BufferDesc k_buffer_desc{
        .type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = sizeof(PerFrameData), .stride = sizeof(PerFrameData)};
    m_per_frame_data_buffer = {*m_gc, k_buffer_desc};
    RNDR_ASSERT(m_per_frame_data_buffer.IsValid(), "Buffer could not be created!");

    const Rndr::BufferDesc vertex_buffer_desc{.type = Rndr::BufferType::ShaderStorage,
                                              .usage = Rndr::Usage::Dynamic,
                                              .size = m_desc.max_char_render_count * k_char_vertex_count * sizeof(VertexData),
                                              .stride = sizeof(VertexData)};
    m_vertex_buffer = {*m_gc, vertex_buffer_desc};
    RNDR_ASSERT(m_vertex_buffer.IsValid(), "Vertex buffer could not be created!");

    const Rndr::BufferDesc index_buffer_desc{.type = Rndr::BufferType::Index,
                                             .usage = Rndr::Usage::Dynamic,
                                             .size = m_desc.max_char_render_count * k_char_index_count * sizeof(u32),
                                             .stride = sizeof(u32)};
    m_index_buffer = {*m_gc, index_buffer_desc};
    RNDR_ASSERT(m_index_buffer.IsValid(), "Buffer could not be created!");

    const Opal::StringUtf8 vertex_shader_source = Rndr::File::ReadShader(RNDR_SAMPLES_SHADERS_DIR, "text-render-atlas.vert");
    const Rndr::ShaderDesc vertex_shader_desc{.type = Rndr::ShaderType::Vertex, .source = vertex_shader_source};
    m_vertex_shader = {*m_gc, vertex_shader_desc};
    RNDR_ASSERT(m_vertex_shader.IsValid(), "Vertex shader could not be created!");

    const Opal::StringUtf8 fragment_shader_source = Rndr::File::ReadShader(RNDR_SAMPLES_SHADERS_DIR, "text-render-atlas.frag");
    const Rndr::ShaderDesc fragment_shader_desc{.type = Rndr::ShaderType::Fragment, .source = fragment_shader_source};
    m_fragment_shader = {*m_gc, fragment_shader_desc};
    RNDR_ASSERT(m_fragment_shader.IsValid(), "Fragment shader could not be created!");

    const Rndr::InputLayoutDesc input_layout_desc = Rndr::InputLayoutBuilder()
                                                        .AddVertexBuffer(m_vertex_buffer, 1, Rndr::DataRepetition::PerVertex)
                                                        .AddIndexBuffer(m_index_buffer)
                                                        .Build();
    const Rndr::PipelineDesc pipeline_desc{
        .vertex_shader = &m_vertex_shader, .pixel_shader = &m_fragment_shader, .input_layout = input_layout_desc};
    m_pipeline = {*m_gc, pipeline_desc};
    RNDR_ASSERT(m_pipeline.IsValid(), "Pipeline could not be created!");

    return true;
}

void BitmapTextRenderer::UpdateFontAtlas()
{
    if (m_font_contents.IsEmpty())
    {
        m_font_contents = Rndr::File::ReadEntireFile(m_desc.font_file_path);
        RNDR_ASSERT(!m_font_contents.IsEmpty(), "Invalid font path");
        const i32 font_count = stbtt_GetNumberOfFonts(m_font_contents.GetData());
        RNDR_ASSERT(font_count > 0, "There must be at least one font defined inside the font file!");
        m_packed_chars.Resize(m_desc.code_point_count);
        m_aligned_quads.Resize(m_desc.code_point_count);
        stbtt_InitFont(&m_font_info, m_font_contents.GetData(), stbtt_GetFontOffsetForIndex(m_font_contents.GetData(), 0));
    }

    Opal::DynamicArray<u8> atlas(k_atlas_height * k_atlas_width);

    // Render font into the atlas
    stbtt_pack_context pack_context{};
    stbtt_PackBegin(&pack_context, atlas.GetData(), k_atlas_width, k_atlas_height, k_atlas_width, 1, nullptr);
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
    // Useful for debugging to dump rasterized atlas
    const Rndr::Bitmap bitmap(k_atlas_width, k_atlas_height, 1, Rndr::PixelFormat::R8_UNORM, Opal::AsWritableBytes(atlas));
    Rndr::File::SaveImage(bitmap, "atlas.png");

    constexpr Rndr::TextureDesc k_texture_desc{.width = k_atlas_width,
                                               .height = k_atlas_height,
                                               .type = Rndr::TextureType::Texture2D,
                                               .pixel_format = Rndr::PixelFormat::R8_UNORM};
    m_atlas_texture = {*m_gc, k_texture_desc, {}, Opal::AsBytes(atlas)};
    RNDR_ASSERT(m_atlas_texture.IsValid(), "Atlas texture could not be created!");
}

void BitmapTextRenderer::Destroy()
{
    m_atlas_texture.Destroy();
    m_index_buffer.Destroy();
    m_vertex_buffer.Destroy();
    m_per_frame_data_buffer.Destroy();
    m_fragment_shader.Destroy();
    m_vertex_shader.Destroy();
    m_pipeline.Destroy();
}

void BitmapTextRenderer::UpdateFrameBuffer(Rndr::FrameBuffer& fb)
{
    m_frame_buffer = &fb;
}

void BitmapTextRenderer::UpdateFontSize(f32 font_size)
{
    if (font_size != m_desc.font_size)
    {
        m_desc.font_size = font_size;
        UpdateFontAtlas();
    }
}

void BitmapTextRenderer::UpdateFontOversampling(u32 oversample_h, u32 oversample_v)
{
    if (m_desc.oversample_h != oversample_h || m_desc.oversample_v != oversample_v)
    {
        m_desc.oversample_h = oversample_h;
        m_desc.oversample_v = oversample_v;
        UpdateFontAtlas();
    }
}

bool BitmapTextRenderer::DrawText(const Opal::StringUtf8& text, const Rndr::Vector2f& in_position, const Rndr::Vector4f& color)
{
    Rndr::Vector2f curr_position = in_position;
    char prev_c = 0;
    for (i32 i = 0; i < text.GetSize(); i++)
    {
        const char c = text[i];
        const stbtt_packedchar* packed_char = &m_packed_chars[c - m_desc.first_code_point];
        const stbtt_aligned_quad* aligned_quad = &m_aligned_quads[c - m_desc.first_code_point];
        Rndr::Vector2f glyph_size;
        glyph_size.x = static_cast<f32>(aligned_quad->x1 - aligned_quad->x0);
        glyph_size.y = static_cast<f32>(aligned_quad->y1 - aligned_quad->y0);

        Rndr::Point2f glyph_bottom_left;
        glyph_bottom_left.x = curr_position.x + packed_char->xoff;
        glyph_bottom_left.y = curr_position.y - (packed_char->yoff + glyph_size.y);

        const Rndr::Point2f glyph_vertices[4] = {{glyph_bottom_left.x, glyph_bottom_left.y},
                                                 {glyph_bottom_left.x + glyph_size.x, glyph_bottom_left.y},
                                                 {glyph_bottom_left.x + glyph_size.x, glyph_bottom_left.y + glyph_size.y},
                                                 {glyph_bottom_left.x, glyph_bottom_left.y + glyph_size.y}};

        const i32 first_vertex_idx = static_cast<i32>(m_vertices.GetSize());
        m_vertices.PushBack({.pos = glyph_vertices[0], .uv = {aligned_quad->s0, aligned_quad->t1}, .color = color});
        m_vertices.PushBack({.pos = glyph_vertices[1], .uv = {aligned_quad->s1, aligned_quad->t1}, .color = color});
        m_vertices.PushBack({.pos = glyph_vertices[2], .uv = {aligned_quad->s1, aligned_quad->t0}, .color = color});
        m_vertices.PushBack({.pos = glyph_vertices[3], .uv = {aligned_quad->s0, aligned_quad->t0}, .color = color});

        m_indices.PushBack(first_vertex_idx + 0);
        m_indices.PushBack(first_vertex_idx + 2);
        m_indices.PushBack(first_vertex_idx + 3);
        m_indices.PushBack(first_vertex_idx + 0);
        m_indices.PushBack(first_vertex_idx + 1);
        m_indices.PushBack(first_vertex_idx + 2);

        const f32 scale = stbtt_ScaleForPixelHeight(&m_font_info, m_desc.font_size);
        const i32 kern = 0;
        if (prev_c != 0)
        {
            stbtt_GetCodepointKernAdvance(&m_font_info, prev_c, c);
        }
        const f32 kern_scaled = kern * scale;

        curr_position.x += packed_char->xadvance + kern_scaled;
        prev_c = c;
    }

    return true;
}

void BitmapTextRenderer::Render(f32 delta_seconds, Rndr::CommandList& cmd_list)
{
    RNDR_UNUSED(delta_seconds);
    PerFrameData per_frame_data;
    const f32 width = static_cast<f32>(m_frame_buffer->GetColorAttachment(0).GetTextureDesc().width);
    const f32 height = static_cast<f32>(m_frame_buffer->GetColorAttachment(0).GetTextureDesc().height);
    per_frame_data.mvp = Rndr::OrthographicOpenGL(0, width, 0, height, -1.0f, 1.0f);
    per_frame_data.mvp = Opal::Transpose(per_frame_data.mvp);

    cmd_list.CmdUpdateBuffer(m_per_frame_data_buffer, Opal::AsBytes(per_frame_data));
    cmd_list.CmdUpdateBuffer(m_vertex_buffer, Opal::AsBytes(m_vertices));
    cmd_list.CmdUpdateBuffer(m_index_buffer, Opal::AsBytes(m_indices));

    cmd_list.CmdBindFrameBuffer(*m_frame_buffer);
    cmd_list.CmdClearColor({0, 0, 0, 0});
    cmd_list.CmdBindTexture(m_atlas_texture, 0);
    cmd_list.CmdBindPipeline(m_pipeline);
    cmd_list.CmdBindBuffer(m_per_frame_data_buffer, 0);

    cmd_list.CmdDrawIndices(Rndr::PrimitiveTopology::Triangle, static_cast<i32>(m_indices.GetSize()), 1, 0);

    m_vertices.Clear();
    m_indices.Clear();
}
