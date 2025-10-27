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

#include "bitmap-text-renderer.hpp"
#include "types.hpp"

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

    BitmapTextRenderer text_renderer;
    BitmapTextRendererDesc text_renderer_desc;
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
    f32 font_size_in_pixels = text_renderer_desc.font_size;
    i32 oversample_h = static_cast<i32>(text_renderer_desc.oversample_h);
    i32 oversample_v = static_cast<i32>(text_renderer_desc.oversample_v);
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
        text_renderer.UpdateFontOversampling(oversample_h, oversample_v);

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
        ImGui::InputInt("Font Oversampling Horizontal", &oversample_h);
        ImGui::InputInt("Font Oversampling Vertical", &oversample_v);
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
