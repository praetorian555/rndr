#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/application.hpp"
#include "rndr/file.hpp"
#include "rndr/fly-camera.hpp"
#include "rndr/imgui-system.hpp"
#include "rndr/log.hpp"
#include "rndr/render-api.hpp"
#include "rndr/types.hpp"

#include "example-controller.h"
#include "imgui.h"
#include "rndr/frames-per-second-counter.h"
#include "rndr/trace.hpp"

struct RNDR_ALIGN(16) Uniforms
{
    Rndr::Matrix4x4f view;
    Rndr::Matrix4x4f projection;
    Rndr::Vector3f position;
};

void SetupFlyCameraControls(Rndr::Application& app, Rndr::FlyCamera& camera);

Rndr::FrameBuffer RecreateFrameBuffer(Rndr::GraphicsContext& gc, Rndr::i32 width, Rndr::i32 height)
{
    const Rndr::FrameBufferDesc desc{.color_attachments = {Rndr::TextureDesc{.width = width, .height = height}},
                                     .color_attachment_samplers = {{}}};
    return {gc, desc};
}

int main()
{
    Rndr::i32 resolution_index = 0;
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
    Rndr::i32 window_width = 0;
    Rndr::i32 window_height = 0;
    Rndr::i32 x = 0;
    Rndr::i32 y = 0;
    window->GetPositionAndSize(x, y, window_width, window_height);
    window->SetTitle("Window Sample");

    app->EnableHighPrecisionCursorMode(true, *window);

    const Rndr::GraphicsContextDesc gc_desc{.window_handle = window->GetNativeHandle()};
    Rndr::GraphicsContext gc{gc_desc};
    const Rndr::SwapChainDesc swap_chain_desc{.width = window_width, .height = window_height, .enable_vsync = true};
    Rndr::SwapChain swap_chain{gc, swap_chain_desc};
    Rndr::FrameBuffer final_render =
        RecreateFrameBuffer(gc, rendering_resolution_options[resolution_index].x, rendering_resolution_options[resolution_index].y);

    app->on_window_resize.Bind(
        [&swap_chain, window](const Rndr::GenericWindow& w, Rndr::i32 width, Rndr::i32 height)
        {
            if (window == &w)
            {
                swap_chain.SetSize(width, height);
            }
        });

    app->GetInputSystemChecked().GetCurrentContext().AddAction(
        "Switch display mode",
        {Rndr::InputBinding::CreateKeyboardButtonBinding(Rndr::InputPrimitive::F2, Rndr::InputTrigger::ButtonPressed,
                                                         [window](Rndr::InputPrimitive, Rndr::InputTrigger, Rndr::f32, bool is_repeated)
                                                         {
                                                             if (!is_repeated)
                                                             {
                                                                 const Rndr::GenericWindowMode current_mode = window->GetMode();
                                                                 window->SetMode(current_mode == Rndr::GenericWindowMode::Windowed
                                                                                     ? Rndr::GenericWindowMode::BorderlessFullscreen
                                                                                     : Rndr::GenericWindowMode::Windowed);
                                                             }
                                                         })});

    Rndr::ImGuiContext imgui_context(*window, gc);
    app->RegisterSystemMessageHandler(&imgui_context);

    const Rndr::BufferDesc buffer_desc{.usage = Rndr::Usage::Dynamic, .size = sizeof(Uniforms), .stride = sizeof(Uniforms)};
    Rndr::Buffer uniform_buffer(gc, buffer_desc);

    const Opal::StringUtf8 vertex_shader_path = Opal::Paths::Combine(nullptr, RNDR_CORE_ASSETS_DIR, "grid.vert").GetValue();
    Rndr::f64 vertex_shader_last_modified_time = Opal::GetLastFileModifiedTimeInSeconds(vertex_shader_path);
    const Opal::StringUtf8 vertex_shader_source = Rndr::File::ReadShader(RNDR_CORE_ASSETS_DIR, "grid.vert");
    const Rndr::ShaderDesc vertex_shader_desc{.type = Rndr::ShaderType::Vertex, .source = vertex_shader_source};
    Rndr::Shader vertex_shader(gc, vertex_shader_desc);

    const Opal::StringUtf8 frag_shader_path = Opal::Paths::Combine(nullptr, RNDR_CORE_ASSETS_DIR, "grid.frag").GetValue();
    Rndr::f64 frag_shader_last_modified_time = Opal::GetLastFileModifiedTimeInSeconds(frag_shader_path);
    const Opal::StringUtf8 fragment_shader_source = Rndr::File::ReadShader(RNDR_CORE_ASSETS_DIR, "grid.frag");
    const Rndr::ShaderDesc fragment_shader_desc{.type = Rndr::ShaderType::Fragment, .source = fragment_shader_source};
    Rndr::Shader fragment_shader(gc, fragment_shader_desc);

    Rndr::Pipeline pipeline;
    if (vertex_shader.IsValid() && fragment_shader.IsValid())
    {
        const Rndr::PipelineDesc desc{.vertex_shader = &vertex_shader, .pixel_shader = &fragment_shader};
        pipeline = Rndr::Pipeline(gc, desc);
    }

    const Rndr::FlyCameraDesc fly_camera_desc{.start_yaw_radians = Opal::k_pi_over_2_float};
    ExampleController controller(*app, window_width, window_height, fly_camera_desc, 10.0f, 0.005f, 0.005f);

    app->on_window_resize.Bind(
        [&controller, window](const Rndr::GenericWindow& w, Rndr::i32 width, Rndr::i32 height)
        {
            if (window == &w)
            {
                controller.SetScreenSize(width, height);
            }
        });

    app->GetInputSystemChecked().GetInputContexts()[0]->AddAction(
        "Toggle movement controls", {Rndr::InputBinding::CreateKeyboardButtonBinding(
                                        Rndr::InputPrimitive::F1, Rndr::InputTrigger::ButtonPressed,
                                        [app, &controller](Rndr::InputPrimitive, Rndr::InputTrigger, Rndr::f32, bool is_repeated)
                                        {
                                            if (!is_repeated)
                                            {
                                                const Rndr::CursorPositionMode mode = app->GetCursorPositionMode();
                                                if (mode == Rndr::CursorPositionMode::Normal)
                                                {
                                                    app->ShowCursor(false);
                                                    app->SetCursorPositionMode(Rndr::CursorPositionMode::ResetToCenter);
                                                }
                                                else
                                                {
                                                    app->ShowCursor(true);
                                                    app->SetCursorPositionMode(Rndr::CursorPositionMode::Normal);
                                                }
                                                controller.Enable(!controller.IsEnabled());
                                            }
                                        })});

    Rndr::FramesPerSecondCounter fps_counter;
    int selected_resolution_index = 0;
    bool stats_window = true;
    Rndr::f64 check_change_time = 0.0;
    Rndr::f32 delta_seconds = 0.016f;
    while (!window->IsClosed())
    {
        const Rndr::f64 start_seconds = Opal::GetSeconds();

        fps_counter.Update(delta_seconds);

        app->ProcessSystemEvents(delta_seconds);

        controller.Tick(delta_seconds);

        if (selected_resolution_index != resolution_index)
        {
            resolution_index = selected_resolution_index;
            final_render.Destroy();
            final_render =
                RecreateFrameBuffer(gc, rendering_resolution_options[resolution_index].x, rendering_resolution_options[resolution_index].y);
        }

        gc.BindSwapChainFrameBuffer(swap_chain);
        gc.ClearAll(Rndr::Colors::k_black);

        gc.BindFrameBuffer(final_render);
        gc.ClearAll(Rndr::Colors::k_black);

        Uniforms uniforms;
        uniforms.view = Opal::Transpose(controller.GetViewTransform());
        uniforms.projection = Opal::Transpose(controller.GetProjectionTransform());
        gc.BindBuffer(uniform_buffer, 0);
        gc.UpdateBuffer(uniform_buffer, Opal::AsBytes(uniforms));

        check_change_time += delta_seconds;
        if (check_change_time >= 1.0f)
        {
            check_change_time = 0.0f;
            const Rndr::f64 vertex_modified_time = Opal::GetLastFileModifiedTimeInSeconds(vertex_shader_path);
            const Rndr::f64 frag_modified_time = Opal::GetLastFileModifiedTimeInSeconds(frag_shader_path);
            const bool did_vertex_shader_change = vertex_modified_time != vertex_shader_last_modified_time;
            const bool did_pixel_shader_change = frag_modified_time != frag_shader_last_modified_time;
            vertex_shader_last_modified_time = vertex_modified_time;
            frag_shader_last_modified_time = frag_modified_time;
            if (did_vertex_shader_change)
            {
                const Opal::StringUtf8 new_vertex_shader_source = Rndr::File::ReadShader(RNDR_CORE_ASSETS_DIR, "grid.vert");
                const Rndr::ShaderDesc new_vertex_shader_desc{.type = Rndr::ShaderType::Vertex, .source = new_vertex_shader_source};
                Rndr::Shader new_shader(gc, new_vertex_shader_desc);
                if (new_shader.IsValid())
                {
                    vertex_shader = Opal::Move(new_shader);
                }
            }
            if (did_pixel_shader_change)
            {
                const Opal::StringUtf8 new_frag_shader_source = Rndr::File::ReadShader(RNDR_CORE_ASSETS_DIR, "grid.frag");
                const Rndr::ShaderDesc new_frag_shader_desc{.type = Rndr::ShaderType::Fragment, .source = new_frag_shader_source};
                Rndr::Shader new_shader(gc, new_frag_shader_desc);
                if (new_shader.IsValid())
                {
                    fragment_shader = Opal::Move(new_shader);
                }
            }
            if ((did_vertex_shader_change || did_pixel_shader_change) && vertex_shader.IsValid() && fragment_shader.IsValid())
            {
                const Rndr::PipelineDesc desc{.vertex_shader = &vertex_shader, .pixel_shader = &fragment_shader};
                pipeline = Rndr::Pipeline(gc, desc);
            }
        }

        if (pipeline.IsValid())
        {
            gc.BindPipeline(pipeline);
            gc.DrawVertices(Rndr::PrimitiveTopology::Triangle, 6);
        }

        const Rndr::BlitFrameBufferDesc blit_desc;
        gc.BlitToSwapChain(swap_chain, final_render, blit_desc);
        gc.BindSwapChainFrameBuffer(swap_chain);

        imgui_context.StartFrame();
        ImGui::Begin("Stats", &stats_window);
        ImGui::Combo("Rendering Resolution", &selected_resolution_index, rendering_resolution_options_str, 3);
        ImGui::Text("Current Rendering resolution: %s", rendering_resolution_options_str[selected_resolution_index]);
        window->GetPositionAndSize(x, y, window_width, window_height);
        ImGui::Text("Window Resolution: %dx%d", window_width, window_height);
        ImGui::Text("FPS: %.2f", fps_counter.GetFramesPerSecond());
        ImGui::Text("Frame Time: %.2f ms", (1 / fps_counter.GetFramesPerSecond()) * 1000.0f);
        ImGui::End();
        imgui_context.EndFrame();

        gc.Present(swap_chain);

        const Rndr::f64 end_seconds = Opal::GetSeconds();
        delta_seconds = static_cast<Rndr::f32>(end_seconds - start_seconds);
    }

    uniform_buffer.Destroy();
    pipeline.Destroy();
    vertex_shader.Destroy();
    fragment_shader.Destroy();
    final_render.Destroy();
    gc.Destroy();
    app->DestroyGenericWindow(window);
    Rndr::Application::Destroy();
    return 0;
}
