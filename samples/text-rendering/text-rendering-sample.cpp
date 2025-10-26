#include "stb_truetype/stb_truetype.h"

#include "opal/time.h"

#include "rndr/application.hpp"
#include "rndr/file.hpp"
#include "rndr/fly-camera.hpp"
#include "rndr/imgui-system.hpp"
#include "rndr/log.hpp"
#include "rndr/render-api.hpp"
#include "rndr/types.hpp"

#include "rndr/frames-per-second-counter.h"

#include "imgui.h"

#include "example-controller.h"
#include "rndr/input-layout-builder.hpp"
#include "rndr/projections.hpp"
#include "types.hpp"

struct RNDR_ALIGN(16) PerFrameData
{
    RNDR_ALIGN(16) Rndr::Matrix4x4f mvp;
};

struct RNDR_ALIGN(16) VertexData
{
    Rndr::Point2f pos;
    Rndr::Point2f uv;
    Rndr::Vector4f color;
};

struct TextRendererDesc
{
    Opal::StringUtf8 font_file_path;
    f32 font_size = 64.0f;
    i32 first_code_point = 32;  // ASCII
    i32 code_point_count = 95;
    i32 max_char_render_count = 1024;
};

class TextRenderer
{
public:
    bool Init(Rndr::GraphicsContext* gc, Rndr::FrameBuffer* frame_buffer, const TextRendererDesc& desc);
    void Destroy();

    void UpdateFrameBuffer(Rndr::FrameBuffer& fb);
    void UpdateFontSize(f32 font_size);

    bool DrawText(const Opal::StringUtf8& text, const Rndr::Vector2f& position, const Rndr::Vector4f& color);

    void Render(f32 delta_seconds, Rndr::CommandList& cmd_list);

private:
    constexpr static i32 k_atlas_width = 1024;
    constexpr static i32 k_atlas_height = 1024;
    constexpr static i32 k_char_vertex_count = 4;
    constexpr static i32 k_char_index_count = 6;

    Rndr::GraphicsContext* m_gc = nullptr;
    Rndr::FrameBuffer* m_frame_buffer = nullptr;
    TextRendererDesc m_desc;
    stbtt_fontinfo m_font_info;
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

Rndr::FrameBuffer RecreateFrameBuffer(Rndr::GraphicsContext& gc, Rndr::i32 width, Rndr::i32 height);

int main()
{
    i32 resolution_index = 2;
    Opal::DynamicArray<Rndr::Vector2i> rendering_resolution_options{{2560, 1440}, {1920, 1080}, {1600, 900}};
    const char* rendering_resolution_options_str[]{"2560x1440", "1920x1080", "1600x900"};

    const Rndr::ApplicationDesc app_desc{.enable_input_system = true};
    Rndr::Application* app = Rndr::Application::Create(app_desc);
    if (app == nullptr)
    {
        RNDR_LOG_ERROR("Failed to create app!");
        return -1;
    }
    Rndr::GenericWindow* window = app->CreateGenericWindow();
    if (window == nullptr)
    {
        RNDR_LOG_ERROR("Failed to create window!");
        Rndr::Application::Destroy();
        return -1;
    }
    i32 window_width = 0;
    i32 window_height = 0;
    i32 x = 0;
    i32 y = 0;
    window->GetPositionAndSize(x, y, window_width, window_height);
    window->SetTitle("Text Rendering Sample");

    const Rndr::GraphicsContextDesc gc_desc{.window_handle = window->GetNativeHandle()};
    Rndr::GraphicsContext gc{gc_desc};
    const Rndr::SwapChainDesc swap_chain_desc{.width = window_width, .height = window_height, .enable_vsync = true};
    Rndr::SwapChain swap_chain{gc, swap_chain_desc};
    Rndr::FrameBuffer final_render =
        RecreateFrameBuffer(gc, rendering_resolution_options[resolution_index].x, rendering_resolution_options[resolution_index].y);

    Rndr::ImGuiContext imgui_context(*window, gc);
    app->RegisterSystemMessageHandler(&imgui_context);

    TextRenderer text_renderer;
    TextRendererDesc text_renderer_desc;
    text_renderer_desc.font_file_path = R"(C:\Windows\Fonts\CascadiaMono.ttf)";
    text_renderer.Init(&gc, &final_render, text_renderer_desc);

    app->on_window_resize.Bind(
        [&swap_chain, window, &text_renderer](const Rndr::GenericWindow& w, Rndr::i32 width, Rndr::i32 height)
        {
            if (window == &w)
            {
                swap_chain.SetSize(width, height);
            }
        });

    Rndr::CommandList present_cmd_list{gc};
    present_cmd_list.CmdPresent(swap_chain);

    Rndr::FramesPerSecondCounter fps_counter;
    int selected_resolution_index = 2;
    bool stats_window = true;
    Rndr::f32 delta_seconds = 0.016f;
    char buffer[1024] = {};
    f32 font_size_in_pixels = 40.0f;
    while (!window->IsClosed())
    {
        const Rndr::f64 start_seconds = Opal::GetSeconds();

        fps_counter.Update(delta_seconds);

        app->ProcessSystemEvents(delta_seconds);

        if (selected_resolution_index != resolution_index)
        {
            resolution_index = selected_resolution_index;
            final_render.Destroy();
            final_render =
                RecreateFrameBuffer(gc, rendering_resolution_options[resolution_index].x, rendering_resolution_options[resolution_index].y);
        }

        text_renderer.UpdateFontSize(font_size_in_pixels);

        text_renderer.DrawText("Hello World!", {100, 100}, Rndr::Colors::k_white);
        text_renderer.DrawText(buffer, {100, 300}, Rndr::Colors::k_white);

        Rndr::CommandList cmd_list{gc};
        cmd_list.CmdBindSwapChainFrameBuffer(swap_chain);
        cmd_list.CmdClearAll(Rndr::Colors::k_pink);

        text_renderer.Render(delta_seconds, cmd_list);

        cmd_list.CmdBlitToSwapChain(swap_chain, final_render, {});
        cmd_list.CmdBindSwapChainFrameBuffer(swap_chain);
        gc.SubmitCommandList(cmd_list);

        imgui_context.StartFrame();
        ImGui::Begin("Stats", &stats_window);
        ImGui::Combo("Rendering Resolution", &selected_resolution_index, rendering_resolution_options_str, 3);
        ImGui::Text("Current Rendering resolution: %s", rendering_resolution_options_str[selected_resolution_index]);
        window->GetPositionAndSize(x, y, window_width, window_height);
        ImGui::Text("Window Resolution: %dx%d", window_width, window_height);
        ImGui::Text("FPS: %.2f", fps_counter.GetFramesPerSecond());
        ImGui::Text("Frame Time: %.2f ms", (1 / fps_counter.GetFramesPerSecond()) * 1000.0f);
        ImGui::InputText("Input Text", buffer, 1024);
        ImGui::InputFloat("Font Size in Pixels", &font_size_in_pixels, 2.0);
        ImGui::End();
        imgui_context.EndFrame();

        gc.SubmitCommandList(present_cmd_list);

        const Rndr::f64 end_seconds = Opal::GetSeconds();
        delta_seconds = static_cast<Rndr::f32>(end_seconds - start_seconds);
    }

    text_renderer.Destroy();
    final_render.Destroy();
    gc.Destroy();

    app->DestroyGenericWindow(window);
    Rndr::Application::Destroy();

    return 0;
}

Rndr::FrameBuffer RecreateFrameBuffer(Rndr::GraphicsContext& gc, Rndr::i32 width, Rndr::i32 height)
{
    const Rndr::FrameBufferDesc desc{.color_attachments = {Rndr::TextureDesc{.width = width, .height = height}},
                                     .color_attachment_samplers = {{}}};
    return {gc, desc};
}

bool TextRenderer::Init(Rndr::GraphicsContext* gc, Rndr::FrameBuffer* frame_buffer, const TextRendererDesc& desc)
{
    m_gc = gc;
    RNDR_ASSERT(m_gc != nullptr, "Invalid graphics context");
    m_frame_buffer = frame_buffer;
    RNDR_ASSERT(m_frame_buffer != nullptr, "Invalid frame buffer");
    m_desc = desc;

    const Opal::DynamicArray<Rndr::u8> font_contents = Rndr::File::ReadEntireFile(m_desc.font_file_path);
    RNDR_ASSERT(!font_contents.IsEmpty(), "Invalid font path");
    const i32 font_count = stbtt_GetNumberOfFonts(font_contents.GetData());
    RNDR_ASSERT(font_count > 0, "There must be at least one font defined inside the font file!");
    RNDR_LOG_INFO("Number of fonts: %d", font_count);

    i32 result = stbtt_InitFont(&m_font_info, font_contents.GetData(), stbtt_GetFontOffsetForIndex(font_contents.GetData(), 0));
    RNDR_ASSERT(result == 1, "Failed to init stbtt font");

    Opal::DynamicArray<u8> atlas(k_atlas_height * k_atlas_width);
    m_packed_chars.Resize(m_desc.code_point_count);
    m_aligned_quads.Resize(m_desc.code_point_count);

    // Render font into the atlas
    stbtt_pack_context pack_context{};
    stbtt_PackBegin(&pack_context, atlas.GetData(), k_atlas_width, k_atlas_height, k_atlas_width, 1, nullptr);
    stbtt_PackFontRange(&pack_context, font_contents.GetData(), 0, m_desc.font_size, m_desc.first_code_point, m_desc.code_point_count,
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

    m_vertices.Reserve(m_desc.max_char_render_count * k_char_vertex_count);
    m_indices.Reserve(m_desc.max_char_render_count * k_char_index_count);

    constexpr Rndr::TextureDesc k_texture_desc{.width = k_atlas_width,
                                               .height = k_atlas_height,
                                               .type = Rndr::TextureType::Texture2D,
                                               .pixel_format = Rndr::PixelFormat::R8_UNORM};
    m_atlas_texture = {*m_gc, k_texture_desc, {}, Opal::AsBytes(atlas)};
    RNDR_ASSERT(m_atlas_texture.IsValid(), "Atlas texture could not be created!");

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

void TextRenderer::Destroy()
{
    m_atlas_texture.Destroy();
    m_index_buffer.Destroy();
    m_vertex_buffer.Destroy();
    m_per_frame_data_buffer.Destroy();
    m_fragment_shader.Destroy();
    m_vertex_shader.Destroy();
    m_pipeline.Destroy();
}

void TextRenderer::UpdateFrameBuffer(Rndr::FrameBuffer& fb)
{
    m_frame_buffer = &fb;
}

void TextRenderer::UpdateFontSize(f32 font_size)
{
    if (m_desc.font_size != font_size)
    {
        m_desc.font_size = font_size;

        const Opal::DynamicArray<Rndr::u8> font_contents = Rndr::File::ReadEntireFile(m_desc.font_file_path);
        RNDR_ASSERT(!font_contents.IsEmpty(), "Invalid font path");
        const i32 font_count = stbtt_GetNumberOfFonts(font_contents.GetData());
        RNDR_ASSERT(font_count > 0, "There must be at least one font defined inside the font file!");

        Opal::DynamicArray<u8> atlas(k_atlas_height * k_atlas_width);

        // Render font into the atlas
        stbtt_pack_context pack_context{};
        stbtt_PackBegin(&pack_context, atlas.GetData(), k_atlas_width, k_atlas_height, k_atlas_width, 1, nullptr);
        stbtt_PackFontRange(&pack_context, font_contents.GetData(), 0, m_desc.font_size, m_desc.first_code_point, m_desc.code_point_count,
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
}

bool TextRenderer::DrawText(const Opal::StringUtf8& text, const Rndr::Vector2f& in_position, const Rndr::Vector4f& color)
{
    f32 pixel_size = 1.0f;  // 2.0f / m_frame_buffer->GetDesc().color_attachments[0].height;
    f32 font_size = 1.0f;
    Rndr::Vector2f curr_position = in_position;
    char prev_c = 0;
    for (i32 i = 0; i < text.GetSize(); i++)
    {
        const char c = text[i];
        const stbtt_packedchar* packed_char = &m_packed_chars[c - m_desc.first_code_point];
        const stbtt_aligned_quad* aligned_quad = &m_aligned_quads[c - m_desc.first_code_point];
        Rndr::Vector2f glyph_size;
        glyph_size.x = f32(packed_char->x1 - packed_char->x0);
        glyph_size.y = f32(packed_char->y1 - packed_char->y0);

        Rndr::Point2f glyph_bottom_left;
        glyph_bottom_left.x = curr_position.x + packed_char->xoff;
        glyph_bottom_left.y = curr_position.y - (packed_char->yoff + packed_char->y1 - packed_char->y0);

        Rndr::Point2f glyph_vertices[4] = {{glyph_bottom_left.x, glyph_bottom_left.y},
                                           {glyph_bottom_left.x + glyph_size.x, glyph_bottom_left.y},
                                           {glyph_bottom_left.x + glyph_size.x, glyph_bottom_left.y + glyph_size.y},
                                           {glyph_bottom_left.x, glyph_bottom_left.y + glyph_size.y}};

        i32 first_vertex_idx = static_cast<i32>(m_vertices.GetSize());
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

        // Get scale for pixel height
        f32 scale = stbtt_ScaleForPixelHeight(&m_font_info, m_desc.font_size);

        // Get kerning between two characters
        i32 kern = 0;
        if (prev_c != 0)
        {
            stbtt_GetCodepointKernAdvance(&m_font_info, prev_c, c);
        }
        f32 kern_scaled = kern * scale;

        curr_position.x += packed_char->xadvance + kern_scaled;
        prev_c = c;
    }

    return true;
}

void PrintMatrix(const Rndr::Matrix4x4f& mat)
{
    RNDR_LOG_INFO("[0] = %2.5f %2.5f %2.5f %2.5f", mat(0, 0), mat(0, 1), mat(0, 2), mat(0, 3));
    RNDR_LOG_INFO("[1] = %2.5f %2.5f %2.5f %2.5f", mat(1, 0), mat(1, 1), mat(1, 2), mat(1, 3));
    RNDR_LOG_INFO("[2] = %2.5f %2.5f %2.5f %2.5f", mat(2, 0), mat(2, 1), mat(2, 2), mat(2, 3));
    RNDR_LOG_INFO("[3] = %2.5f %2.5f %2.5f %2.5f", mat(3, 0), mat(3, 1), mat(3, 2), mat(3, 3));
}

void TextRenderer::Render(f32 delta_seconds, Rndr::CommandList& cmd_list)
{
    RNDR_UNUSED(delta_seconds);
    PerFrameData per_frame_data;
    const i32 width = m_frame_buffer->GetColorAttachment(0).GetTextureDesc().width;
    const i32 height = m_frame_buffer->GetColorAttachment(0).GetTextureDesc().height;
    per_frame_data.mvp = Rndr::OrthographicOpenGL(0, width, 0, height, -1.0f, 1.0f);
    per_frame_data.mvp = Opal::Transpose(per_frame_data.mvp);

    cmd_list.CmdUpdateBuffer(m_per_frame_data_buffer, Opal::AsWritableBytes(per_frame_data.mvp));
    cmd_list.CmdUpdateBuffer(m_vertex_buffer, Opal::AsWritableBytes(m_vertices));
    cmd_list.CmdUpdateBuffer(m_index_buffer, Opal::AsWritableBytes(m_indices));

    cmd_list.CmdBindFrameBuffer(*m_frame_buffer);
    cmd_list.CmdClearColor(Rndr::Colors::k_black);
    cmd_list.CmdBindTexture(m_atlas_texture, 0);
    cmd_list.CmdBindPipeline(m_pipeline);
    cmd_list.CmdBindBuffer(m_per_frame_data_buffer, 0);

    cmd_list.CmdDrawIndices(Rndr::PrimitiveTopology::Triangle, static_cast<u32>(m_indices.GetSize()), 1, 0);

    m_vertices.Clear();
    m_indices.Clear();
}
