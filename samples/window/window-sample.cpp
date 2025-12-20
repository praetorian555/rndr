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
#include "opal/rng.h"
#include "rndr/frames-per-second-counter.h"
#include "rndr/renderers/grid-renderer.hpp"
#include "rndr/renderers/shape-3d-renderer.hpp"
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
                                     .color_attachment_samplers = {{}},
                                     .use_depth_stencil = true,
                                     .depth_stencil_attachment = {Rndr::TextureDesc{
                                         .width = width, .height = height, .pixel_format = Rndr::PixelFormat::D24_UNORM_S8_UINT}},
                                     .depth_stencil_sampler = {{}},
                                     .debug_name = "Window Sample - Final Render Frame Buffer"};
    return {gc, desc};
}

void DrawScene(Rndr::Shape3DRenderer& shape_renderer, const Rndr::MaterialRegistry& mat_registry);

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

    Rndr::GridRenderer grid_renderer("Grid Renderer", {Opal::Ref{gc}, Opal::Ref{swap_chain}}, Opal::Ref{final_render});
    Rndr::Shape3DRenderer shape_renderer("3D Shape Renderer", {Opal::Ref{gc}, Opal::Ref{swap_chain}}, Opal::Ref{final_render});

    Rndr::MaterialRegistry material_registry(Opal::Ref{gc});
    material_registry.Register("Red Color Material", {.albedo_color = Rndr::Colors::k_red});
    material_registry.Register("White Color Material", {.albedo_color = Rndr::Colors::k_white});
    Opal::StringUtf8 albedo_texture_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "default-texture.png");
    material_registry.Register("Default Material", {.albedo_texture_path = albedo_texture_path});

    const Rndr::FlyCameraDesc fly_camera_desc{.start_position = {0.0f, 1.0f, 0.0f}, .start_yaw_radians = 0};
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

    Rndr::CommandList present_cmd_list{gc};
    present_cmd_list.CmdPresent(swap_chain);

    Rndr::FramesPerSecondCounter fps_counter;
    int selected_resolution_index = 0;
    bool vsync = true;
    bool stats_window = true;
    Rndr::f32 delta_seconds = 0.016f;
    while (!window->IsClosed())
    {
        const Rndr::f64 start_seconds = Opal::GetSeconds();

        fps_counter.Update(delta_seconds);

        app->ProcessSystemEvents(delta_seconds);

        controller.Tick(delta_seconds);
        grid_renderer.SetTransforms(controller.GetViewTransform(), controller.GetProjectionTransform());

        if (selected_resolution_index != resolution_index)
        {
            resolution_index = selected_resolution_index;
            final_render.Destroy();
            final_render =
                RecreateFrameBuffer(gc, rendering_resolution_options[resolution_index].x, rendering_resolution_options[resolution_index].y);
            grid_renderer.SetFrameBufferTarget(Opal::Ref{final_render});
            shape_renderer.SetFrameBufferTarget(Opal::Ref{final_render});
        }

        swap_chain.SetVerticalSync(vsync);

        Rndr::CommandList cmd_list{gc};

        cmd_list.CmdBindFrameBuffer(final_render);
        cmd_list.CmdClearAll(Rndr::Colors::k_black);

        grid_renderer.Render(delta_seconds, cmd_list);

        DrawScene(shape_renderer, material_registry);
        shape_renderer.SetTransforms(controller.GetViewTransform(), controller.GetProjectionTransform());
        shape_renderer.Render(delta_seconds, cmd_list);

        const Rndr::BlitFrameBufferDesc blit_desc;
        cmd_list.CmdBlitToSwapChain(swap_chain, final_render, blit_desc);
        cmd_list.CmdBindSwapChainFrameBuffer(swap_chain);

        gc.SubmitCommandList(cmd_list);

        imgui_context.StartFrame();
        ImGui::Begin("Stats", &stats_window);
        ImGui::Combo("Rendering Resolution", &selected_resolution_index, rendering_resolution_options_str, 3);
        ImGui::Checkbox("Vertical Sync", &vsync);
        ImGui::Text("Current Rendering resolution: %s", rendering_resolution_options_str[selected_resolution_index]);
        window->GetPositionAndSize(x, y, window_width, window_height);
        ImGui::Text("Window Resolution: %dx%d", window_width, window_height);
        ImGui::Text("Cursor mode: %s", app->GetCursorPositionMode() == Rndr::CursorPositionMode::Normal ? "Normal" : "Reset to Center");
        ImGui::Text("Display mode: %s", window->GetMode() == Rndr::GenericWindowMode::Windowed ? "Windowed" : "Borderless Fullscreen");
        ImGui::Text("FPS: %.2f", fps_counter.GetFramesPerSecond());
        ImGui::Text("Frame Time: %.2f ms", (1 / fps_counter.GetFramesPerSecond()) * 1000.0f);
        ImGui::Text("Controls:");
        ImGui::Text("F1 - Toggle between Normal and ResetToCenter cursor position mode");
        ImGui::Text("F2 - Toggle between Windowed and BorderlessFullscreen mode");
        ImGui::End();
        imgui_context.EndFrame();

        gc.SubmitCommandList(present_cmd_list);

        const Rndr::f64 end_seconds = Opal::GetSeconds();
        delta_seconds = static_cast<Rndr::f32>(end_seconds - start_seconds);
    }

    shape_renderer.Destroy();
    grid_renderer.Destroy();
    final_render.Destroy();
    gc.Destroy();

    app->DestroyGenericWindow(window);
    Rndr::Application::Destroy();

    return 0;
}

Rndr::Vector4f RandomColor(Opal::RNG& rng)
{
    return {rng.RandomF32(0, 1.0), rng.RandomF32(0, 1.0), rng.RandomF32(0, 1.0), 1.0f};
}

void DrawScene(Rndr::Shape3DRenderer& shape_renderer, const Rndr::MaterialRegistry& mat_registry)
{
    const Opal::Ref<const Rndr::Material> red_material = mat_registry.Get("Red Color Material");
    const Opal::Ref<const Rndr::Material> white_material = mat_registry.Get("White Color Material");
    const Opal::Ref<const Rndr::Material> default_material = mat_registry.Get("Default Material");
    const Rndr::Matrix4x4f cube_transform = Opal::Translate(Rndr::Vector3f{-2.0f, 0.0f, -10.0f});
    const Rndr::Matrix4x4f sphere_transform = Opal::Translate(Rndr::Vector3f{2.0f, 0.0f, -10.0f});
    shape_renderer.DrawSphere(sphere_transform, default_material);
    shape_renderer.DrawCube(cube_transform, default_material);

    constexpr Rndr::i32 k_cube_size = 10;
    const Rndr::Point3f start_position = {0.0f, 0.0f, 10.0f};
    Opal::RNG rng(100);
    for (Rndr::i32 x = 0; x < k_cube_size; ++x)
    {
        for (Rndr::i32 y = 0; y < k_cube_size; ++y)
        {
            for (Rndr::i32 z = 0; z < k_cube_size; ++z)
            {
                constexpr Rndr::f32 k_distance = 5.0f;
                const Rndr::Point3f draw_location = start_position + Rndr::Vector3f{x * k_distance, y * k_distance, z * k_distance};
                shape_renderer.DrawSphere(Opal::Translate(draw_location), white_material);
            }
        }
    }
}
