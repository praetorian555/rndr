#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/file.hpp"
#include "rndr/frames-per-second-counter.hpp"
#include "rndr/imgui-system.hpp"
#include "rndr/log.hpp"
#include "rndr/types.hpp"

#include "imgui.h"

#include "rndr/canvas/bitmap-text-renderer.hpp"
#include "rndr/canvas/draw-list.hpp"
#include "rndr/canvas/shape-renderer.hpp"

int main()
{
    using namespace Rndr;

    const ApplicationDesc app_desc{.enable_input_system = true};
    auto app = Application::Create(app_desc);
    RNDR_ASSERT(app.IsValid(), "Failed to create Rndr app!");
    constexpr i32 k_width = 1920;
    constexpr i32 k_height = 1080;
    const Rndr::GenericWindowDesc window_desc{.width = k_width, .height = k_height, .name = "Text Rendering Sample"};
    auto window = app->CreateGenericWindow(window_desc);
    RNDR_ASSERT(window.IsValid(), "Failed to create a window!");

    auto context = Canvas::Context::Init(window.Clone());
    RNDR_ASSERT(context.IsValid(), "Failed to create Canvas context!");

    Opal::StringUtf8 font_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "OpenSans.ttf");
    Rndr::ImGuiContext imgui_context(*app, window.Clone(), {.font_path = font_path.Clone()});

    Canvas::BitmapTextRenderer text_renderer;
    Canvas::BitmapTextRendererDesc text_renderer_desc{.font_size = 16.0};
    text_renderer_desc.font_file_path = std::move(font_path);
    text_renderer.Init(context, text_renderer_desc);

    Canvas::ShapeRenderer shape_renderer(context);

    app->on_window_resize.Bind(
        [&context, &window](const Rndr::GenericWindow& w, Rndr::i32 width, Rndr::i32 height)
        {
            if (window.GetPtr() == &w)
            {
                context.Resize(width, height);
            }
        });

    Rndr::FramesPerSecondCounter fps_counter;
    bool stats_window = true;
    Rndr::f32 delta_seconds = 0.016f;
    char buffer[1024] = {};
    f32 font_size_in_pixels = text_renderer_desc.font_size;
    i32 oversample_h = static_cast<i32>(text_renderer_desc.oversample_h);
    i32 oversample_v = static_cast<i32>(text_renderer_desc.oversample_v);
    f32 alpha_multiplier = text_renderer_desc.alpha_multiplier;
    while (!window->IsClosed())
    {
        const Rndr::f64 start_seconds = Opal::GetSeconds();

        fps_counter.Update(delta_seconds);

        app->ProcessSystemEvents(delta_seconds);

        Canvas::DrawList draw_list;
        draw_list.SetRenderTarget(context);
        draw_list.Clear({0.05, 0.02, 0.1, 1}, 1);

        text_renderer.UpdateFontSize(font_size_in_pixels);
        text_renderer.UpdateFontOversampling(oversample_h, oversample_v);
        text_renderer.SetAlphaMultiplier(alpha_multiplier);

        text_renderer.BeginFrame();
        text_renderer.DrawText("The quick brown fox jumps over the lazy dog!", {100, 100}, Rndr::Colors::k_white);
        text_renderer.DrawText(buffer, {100, 300}, Rndr::Colors::k_white);
        text_renderer.Render(draw_list);

        shape_renderer.BeginFrame();
        shape_renderer.DrawRect({50, 400}, {200, 100}, Rndr::Colors::k_red);
        shape_renderer.DrawCircle({400, 500}, 50, Rndr::Colors::k_green);
        shape_renderer.DrawTriangle({600, 400}, {700, 550}, {500, 550}, Rndr::Colors::k_blue);
        shape_renderer.DrawLine({50, 600}, {750, 600}, Rndr::Colors::k_yellow, 3);
        shape_renderer.Render(draw_list);

        draw_list.Execute();

        imgui_context.StartFrame();
        ImGui::Begin("Stats", &stats_window);
        const auto window_size = window->GetSize();
        ImGui::Text("Window Resolution: %dx%d", window_size.x, window_size.y);
        ImGui::Text("FPS: %.2f", fps_counter.GetFramesPerSecond());
        ImGui::Text("Frame Time: %.2f ms", (1 / fps_counter.GetFramesPerSecond()) * 1000.0f);
        ImGui::InputText("Input Text", buffer, 1024);
        ImGui::InputFloat("Font Size in Pixels", &font_size_in_pixels, 2.0);
        ImGui::InputInt("Font Oversampling Horizontal", &oversample_h);
        ImGui::InputInt("Font Oversampling Vertical", &oversample_v);
        ImGui::InputFloat("Alpha multiplier", &alpha_multiplier);
        ImGui::End();
        imgui_context.EndFrame();

        context.Present();

        const Rndr::f64 end_seconds = Opal::GetSeconds();
        delta_seconds = static_cast<Rndr::f32>(end_seconds - start_seconds);
    }

    return 0;
}
