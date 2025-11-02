#include <algorithm>

#include "stb_truetype/stb_truetype.h"

#include "opal/time.h"

#include "rndr/application.hpp"
#include "rndr/file.hpp"
#include "rndr/fly-camera.hpp"
#include "rndr/frames-per-second-counter.h"
#include "rndr/imgui-system.hpp"
#include "rndr/log.hpp"
#include "rndr/render-api.hpp"
#include "rndr/types.hpp"

#include "imgui.h"

#include "../../build/opengl-msvc-opt-debug/_deps/opal-src/include/opal/paths.h"
#include "bitmap-text-renderer.hpp"
#include "shape-2d-renderer.hpp"
#include "types.hpp"

Rndr::FrameBuffer RecreateFrameBuffer(Rndr::GraphicsContext& gc, Rndr::i32 width, Rndr::i32 height, i32 sample_count = 1);

int main()
{
    i32 resolution_index = 1;
    Opal::DynamicArray<Rndr::Vector2i> rendering_resolution_options{{2560, 1440}, {1920, 1080}, {1600, 900}, {1024, 768}};
    const char* rendering_resolution_options_str[]{"2560x1440", "1920x1080", "1600x900", "1024x768"};

    const Rndr::ApplicationDesc app_desc{.enable_input_system = true};
    Rndr::Application* app = Rndr::Application::Create(app_desc);
    if (app == nullptr)
    {
        RNDR_LOG_ERROR("Failed to create app!");
        return -1;
    }
    const Rndr::GenericWindowDesc window_desc{.width = 1920, .height = 1080};
    Rndr::GenericWindow* window = app->CreateGenericWindow(window_desc);
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

    i32 sample_count = 8;

    const Rndr::GraphicsContextDesc gc_desc{.window_handle = window->GetNativeHandle()};
    Rndr::GraphicsContext gc{gc_desc};
    const Rndr::SwapChainDesc swap_chain_desc{.width = window_width, .height = window_height, .enable_vsync = true};
    Rndr::SwapChain swap_chain{gc, swap_chain_desc};
    Rndr::FrameBuffer final_render =
        RecreateFrameBuffer(gc, rendering_resolution_options[resolution_index].x, rendering_resolution_options[resolution_index].y, sample_count);

    Rndr::ImGuiContext imgui_context(*window, gc);
    app->RegisterSystemMessageHandler(&imgui_context);

    BitmapTextRenderer text_renderer;
    BitmapTextRendererDesc text_renderer_desc{.font_size = 16.0};
    text_renderer_desc.font_file_path = Opal::Paths::Combine(nullptr, RNDR_CORE_ASSETS_DIR, "OpenSans.ttf").GetValue();
    text_renderer.Init(&gc, &final_render, text_renderer_desc);

    Shape2DRenderer shape_renderer;
    shape_renderer.Init(&gc, final_render.GetWidth(), final_render.GetHeight());

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
    int selected_resolution_index = 1;
    bool stats_window = true;
    i32 selected_sample_count = sample_count;
    Rndr::f32 delta_seconds = 0.016f;
    char buffer[1024] = {};
    f32 font_size_in_pixels = text_renderer_desc.font_size;
    i32 oversample_h = static_cast<i32>(text_renderer_desc.oversample_h);
    i32 oversample_v = static_cast<i32>(text_renderer_desc.oversample_v);
    char glyph_to_draw[2] = {};
    bool align_to_int = false;
    while (!window->IsClosed())
    {
        const Rndr::f64 start_seconds = Opal::GetSeconds();

        fps_counter.Update(delta_seconds);

        app->ProcessSystemEvents(delta_seconds);

        if (selected_resolution_index != resolution_index || selected_sample_count != sample_count)
        {
            resolution_index = selected_resolution_index;
            sample_count = selected_sample_count;
            final_render.Destroy();
            final_render =
                RecreateFrameBuffer(gc, rendering_resolution_options[resolution_index].x, rendering_resolution_options[resolution_index].y, selected_sample_count);
            shape_renderer.SetFrameBufferSize(final_render.GetWidth(), final_render.GetHeight());
        }

        text_renderer.UpdateFontSize(font_size_in_pixels);
        text_renderer.UpdateFontOversampling(oversample_h, oversample_v);
        text_renderer.UpdateAlignToInt(align_to_int);

        text_renderer.DrawText("The quick brown fox jumps over the lazy dog!", {100, 100}, Rndr::Colors::k_white);
        text_renderer.DrawText(buffer, {100, 300}, Rndr::Colors::k_white);

        if (glyph_to_draw[0] != 0)
        {
            text_renderer.DrawGlyphBitmap(shape_renderer, glyph_to_draw[0], {700, 200}, 800, align_to_int);
        }

        shape_renderer.DrawArrow({700, 100}, {1, 0}, Rndr::Colors::k_white, 100, 2, 10, 7);

        Rndr::CommandList cmd_list{gc};
        cmd_list.CmdBindSwapChainFrameBuffer(swap_chain);
        cmd_list.CmdClearAll(Rndr::Colors::k_pink);
        cmd_list.CmdBindFrameBuffer(final_render);
        cmd_list.CmdClearAll(Rndr::Colors::k_black);

        text_renderer.Render(delta_seconds, cmd_list);
        shape_renderer.Render(delta_seconds, cmd_list);

        cmd_list.CmdBlitToSwapChain(swap_chain, final_render, {});
        cmd_list.CmdBindSwapChainFrameBuffer(swap_chain);
        gc.SubmitCommandList(cmd_list);

        imgui_context.StartFrame();
        ImGui::Begin("Stats", &stats_window);
        ImGui::Combo("Rendering Resolution", &selected_resolution_index, rendering_resolution_options_str, 4);
        ImGui::Text("Current Rendering resolution: %s", rendering_resolution_options_str[selected_resolution_index]);
        window->GetPositionAndSize(x, y, window_width, window_height);
        ImGui::Text("Window Resolution: %dx%d", window_width, window_height);
        ImGui::InputInt("Rendering sample count", &selected_sample_count);
        ImGui::Text("FPS: %.2f", fps_counter.GetFramesPerSecond());
        ImGui::Text("Frame Time: %.2f ms", (1 / fps_counter.GetFramesPerSecond()) * 1000.0f);
        ImGui::InputText("Input Text", buffer, 1024);
        ImGui::InputFloat("Font Size in Pixels", &font_size_in_pixels, 2.0);
        ImGui::InputInt("Font Oversampling Horizontal", &oversample_h);
        ImGui::InputInt("Font Oversampling Vertical", &oversample_v);
        ImGui::InputText("Glyph to inspect", glyph_to_draw, 2);
        ImGui::Checkbox("Align to int", &align_to_int);
        ImGui::End();
        imgui_context.EndFrame();

        gc.SubmitCommandList(present_cmd_list);

        const Rndr::f64 end_seconds = Opal::GetSeconds();
        delta_seconds = static_cast<Rndr::f32>(end_seconds - start_seconds);
    }

    shape_renderer.Destroy();
    text_renderer.Destroy();
    final_render.Destroy();
    gc.Destroy();

    app->DestroyGenericWindow(window);
    Rndr::Application::Destroy();

    return 0;
}

Rndr::FrameBuffer RecreateFrameBuffer(Rndr::GraphicsContext& gc, Rndr::i32 width, Rndr::i32 height, i32 sample_count)
{
    const Rndr::FrameBufferDesc desc{.color_attachments = {Rndr::TextureDesc{.width = width, .height = height, .sample_count = sample_count}},
                                     .color_attachment_samplers = {{}}};
    return {gc, desc};
}
