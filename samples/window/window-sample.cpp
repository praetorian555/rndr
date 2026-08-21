#include "opal/paths.h"
#include "opal/time.h"

#include "../../include/rndr/canvas/renderers/cubemap-renderer.hpp"
#include "../../include/rndr/canvas/renderers/grid-renderer.hpp"
#include "../../include/rndr/canvas/renderers/pbr-renderer.hpp"
#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/draw-list.hpp"
#include "rndr/canvas/projections.hpp"
#include "rndr/canvas/texture.hpp"
#include "rndr/fly-camera.hpp"
#include "rndr/frames-per-second-counter.hpp"
#include "rndr/imgui-system.hpp"
#include "rndr/log.hpp"
#include "rndr/types.hpp"

#include "example-controller.h"
#include "imgui.h"
#include "opal/rng.h"

void SetupFlyCameraControls(Rndr::Application& app, Rndr::FlyCamera& camera);

void DrawScene(Rndr::Canvas::PbrRenderer& renderer, const Rndr::Canvas::Texture& default_albedo_texture,
               const Rndr::Canvas::PbrModel& helmet_model);

int main()
{
    using namespace Rndr;

    const ApplicationDesc app_desc{.enable_input_system = true};
    auto app = Application::Create(app_desc);
    RNDR_ASSERT(app.IsValid(), "Failed to create Rndr app!");

    const GenericWindowDesc window_desc{.name = "Window Sample"};
    auto window = app->CreateGenericWindow(window_desc);
    RNDR_ASSERT(window.IsValid(), "Failed to create a window!");
    window->EnableHighPrecisionCursorMode(true);

    auto context = Canvas::Context::CreateContext(window.Clone());
    RNDR_ASSERT(context.IsValid(), "Failed to create Canvas context!");

    Rndr::ImGuiContext imgui_context(*app, window.Clone());

    Canvas::GridRenderer grid_renderer(Opal::Ref{context});
    Canvas::PbrRenderer pbr_renderer(Opal::Ref{context});
    Canvas::CubemapRenderer cubemap_renderer(Opal::Ref{context});

    // TODO: Replace with actual path to equirectangular image.
    const Opal::StringUtf8 skybox_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "Panorama_Sky_04-512x512.png").GetValue();
    Canvas::TextureDesc skybox_desc;
    skybox_desc.use_mips = true;
    cubemap_renderer.SetEquirectangular(skybox_path, 0, skybox_desc);

    // Load helmet model (mesh + material + textures).
    const Canvas::TextureDesc tex_desc{
        .wrap_u = Canvas::TextureWrap::Repeat,
        .wrap_v = Canvas::TextureWrap::Repeat,
    };
    const Opal::StringUtf8 helmet_path =
        Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "DamagedHelmet", "gltf", "DamagedHelmet.gltf").GetValue();
    Canvas::PbrModel helmet_model = pbr_renderer.LoadModel(helmet_path, tex_desc, true);

    // Load default albedo texture for simple shapes.
    Opal::StringUtf8 default_albedo_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "default-texture.png").GetValue();
    default_albedo_path = Opal::Paths::NormalizePath(std::move(default_albedo_path)).GetValue();
    Canvas::Texture default_albedo_texture = Canvas::Texture::FromFile(default_albedo_path, tex_desc, true);

    // Fly camera.
    const i32 window_width = window->GetSize().x;
    const i32 window_height = window->GetSize().y;
    const FlyCameraDesc fly_camera_desc{.start_position = {0.0f, 1.0f, 0.0f}, .start_yaw_radians = 0};
    ExampleController controller(*app, window_width, window_height, fly_camera_desc, 10.0f, 0.005f, 0.005f);
    controller.Enable(false);

    app->on_window_resize.Bind(
        [&context, &window, &controller](const GenericWindow& w, i32 width, i32 height)
        {
            if (window.GetPtr() == &w)
            {
                context.Resize(width, height);
                controller.SetScreenSize(width, height);
            }
        });

    app->GetInputSystemChecked()
        .GetContextByName("Default")
        .AddAction("Switch display mode")
        .Bind(Key::F2, Trigger::Pressed)
        .OnButton(
            [&window](Trigger, bool is_repeated)
            {
                if (!is_repeated)
                {
                    const GenericWindowMode current_mode = window->GetMode();
                    window->SetMode(current_mode == GenericWindowMode::Windowed ? GenericWindowMode::BorderlessFullscreen
                                                                                : GenericWindowMode::Windowed);
                }
            });

    app->GetInputSystemChecked()
        .GetContextByName("Default")
        .AddAction("Toggle movement controls")
        .Bind(Key::F1, Trigger::Pressed)
        .OnButton(
            [&app, &window, &controller](Trigger, bool is_repeated)
            {
                if (!is_repeated)
                {
                    const CursorPositionMode mode = window->GetCursorPositionMode();
                    if (mode == CursorPositionMode::Normal)
                    {
                        app->ShowCursor(false);
                        window->SetCursorPositionMode(CursorPositionMode::ResetToCenter);
                    }
                    else
                    {
                        app->ShowCursor(true);
                        window->SetCursorPositionMode(CursorPositionMode::Normal);
                    }
                    controller.Enable(!controller.IsEnabled());
                }
            });

    app->GetInputSystemChecked()
        .GetContextByName("Default")
        .AddAction("Exit")
        .Bind(Key::Escape, Trigger::Pressed)
        .OnButton([&window](Trigger, bool) { window->RequestClose(); });

    u32 draw_flags = 0;
    app->GetInputSystemChecked()
        .GetContextByName("Default")
        .AddAction("Lighting Options")
        .Bind(Key::F3, Trigger::Pressed)
        .OnButton(
            [&draw_flags](Trigger, bool)
            {
                draw_flags++;
                draw_flags %= 3;
            });

    // Gamepad. The bindings carry no gamepad index, so they match whichever slot the pad shows up on.
    f32 gamepad_left_stick_x = 0.0f;
    f32 gamepad_left_stick_y = 0.0f;

    app->on_gamepad_connection_change.Bind([](u8 gamepad_index, bool is_connected)
                                           { RNDR_LOG_INFO("Gamepad %d %s", gamepad_index, is_connected ? "connected" : "disconnected"); });

    app->GetInputSystemChecked()
        .GetContextByName("Default")
        .AddAction("Gamepad switch display mode")
        .Bind(GamepadButton::Start, Trigger::Pressed)
        .OnGamepadButton(
            [&window](GamepadButton, Trigger, u8)
            {
                const GenericWindowMode current_mode = window->GetMode();
                window->SetMode(current_mode == GenericWindowMode::Windowed ? GenericWindowMode::BorderlessFullscreen
                                                                            : GenericWindowMode::Windowed);
            });

    app->GetInputSystemChecked()
        .GetContextByName("Default")
        .AddAction("Gamepad exit")
        .Bind(GamepadButton::Back, Trigger::Pressed)
        .OnGamepadButton([&window](GamepadButton, Trigger, u8) { window->RequestClose(); });

    app->GetInputSystemChecked()
        .GetContextByName("Default")
        .AddAction("Gamepad left stick")
        .Bind(GamepadAxis::LeftStickX)
        .Bind(GamepadAxis::LeftStickY)
        .OnGamepadAxis(
            [&gamepad_left_stick_x, &gamepad_left_stick_y](GamepadAxis axis, f32 value, u8)
            {
                if (axis == GamepadAxis::LeftStickX)
                {
                    gamepad_left_stick_x = value;
                }
                else
                {
                    gamepad_left_stick_y = value;
                }
            });

    FramesPerSecondCounter fps_counter;
    bool stats_window = true;
    f32 delta_seconds = 0.016f;
    f32 angle_radians = 0.0f;
    bool use_light = true;
    while (!window->IsClosed())
    {
        const f64 start_seconds = Opal::GetSeconds();

        fps_counter.Update(delta_seconds);

        app->ProcessSystemEvents();
        app->GetInputSystemChecked().ProcessSystemEvents(delta_seconds);
        controller.Tick(delta_seconds);

        Canvas::DrawList draw_list;
        draw_list.SetRenderTarget(context);
        draw_list.Clear(Colors::k_black, 1.0f);

        const Matrix4x4f vp = controller.GetProjectionTransform() * controller.GetViewTransform();

        // Skybox (rendered first, no depth write).
        const Matrix4x4f inverse_vp = Opal::Inverse(vp).GetValue();
        cubemap_renderer.Render(draw_list, inverse_vp);

        pbr_renderer.BeginFrame();
        pbr_renderer.SetViewProjection(vp);
        pbr_renderer.SetCameraPosition(controller.GetCameraPosition());
        pbr_renderer.SetDrawFlags(draw_flags);

        if (use_light)
        {
            constexpr Rndr::Point3f red_light_position(-20, 5, 0);
            pbr_renderer.AddPointLight(Opal::RotateY(Opal::Degrees(angle_radians)) * red_light_position, Rndr::Colors::k_white);
        }

        DrawScene(pbr_renderer, default_albedo_texture, helmet_model);

        pbr_renderer.Render(draw_list);

        grid_renderer.Render(draw_list, controller.GetViewTransform(), controller.GetProjectionTransform());

        draw_list.Execute();

        imgui_context.StartFrame();
        ImGui::Begin("Stats", &stats_window);
        const auto window_size = window->GetSize();
        ImGui::Text("Window Resolution: %dx%d", window_size.x, window_size.y);
        ImGui::Text("FPS: %.2f", fps_counter.GetFramesPerSecond());
        ImGui::Text("Frame Time: %.2f ms", (1 / fps_counter.GetFramesPerSecond()) * 1000.0f);
        ImGui::Text("Controls:");
        ImGui::Text("F1 - Toggle camera controls");
        ImGui::Text("F2 - Toggle fullscreen");
        ImGui::Text("Gamepad Start - Toggle fullscreen, Back - Exit");
        ImGui::Text("Gamepad slot 0: %s", app->IsGamepadConnected(0) ? "connected" : "disconnected");
        ImGui::Text("Gamepad left stick: %.2f, %.2f", gamepad_left_stick_x, gamepad_left_stick_y);
        ImGui::SliderAngle("Position of the light", &angle_radians, 0, 360);
        ImGui::Checkbox("Use light", &use_light);
        if (ImGui::CollapsingHeader("Window Decorations"))
        {
            bool has_title_bar = window->HasTitleBar();
            if (ImGui::Checkbox("Title bar", &has_title_bar))
            {
                window->SetTitleBarVisible(has_title_bar);
            }
            bool has_border = window->HasBorder();
            if (ImGui::Checkbox("Border", &has_border))
            {
                window->SetBorderVisible(has_border);
            }
            bool is_resizable = window->IsResizable();
            if (ImGui::Checkbox("Resizable", &is_resizable))
            {
                window->SetResizable(is_resizable);
            }
            bool supports_minimize = window->IsMinimizeSupported();
            if (ImGui::Checkbox("Minimize button", &supports_minimize))
            {
                window->SetMinimizeSupported(supports_minimize);
            }
            bool supports_maximize = window->IsMaximizeSupported();
            if (ImGui::Checkbox("Maximize button", &supports_maximize))
            {
                window->SetMaximizeSupported(supports_maximize);
            }
            bool supports_close = window->IsCloseSupported();
            if (ImGui::Checkbox("Close button", &supports_close))
            {
                window->SetCloseSupported(supports_close);
            }
            bool show_in_taskbar = window->IsVisibleInTaskbar();
            if (ImGui::Checkbox("Show in task bar", &show_in_taskbar))
            {
                window->SetVisibleInTaskbar(show_in_taskbar);
            }
            bool always_on_top = window->IsAlwaysOnTop();
            if (ImGui::Checkbox("Always on top", &always_on_top))
            {
                window->SetAlwaysOnTop(always_on_top);
            }
        }
        ImGui::End();
        imgui_context.EndFrame();

        context.Present();

        const f64 end_seconds = Opal::GetSeconds();
        delta_seconds = static_cast<f32>(end_seconds - start_seconds);
    }

    return 0;
}

void DrawScene(Rndr::Canvas::PbrRenderer& renderer, const Rndr::Canvas::Texture& default_albedo_texture,
               const Rndr::Canvas::PbrModel& helmet_model)
{

    // Green cube.
    const Rndr::Canvas::PbrMaterialDesc green_material{.material_name = "Green Material", .albedo_color = Rndr::Colors::k_green};
    const Rndr::Matrix4x4f cube_transform = Opal::Translate(Rndr::Vector3f{-2.0f, 0.0f, -10.0f});
    renderer.DrawCube(cube_transform, green_material);

    // Textured sphere.
    const Rndr::Canvas::PbrMaterialDesc default_material{.material_name = "Default Material",
                                                         .albedo_texture = Opal::Ref<const Rndr::Canvas::Texture>(default_albedo_texture)};
    const Rndr::Matrix4x4f sphere_transform = Opal::Translate(Rndr::Vector3f{2.0f, 0.0f, -10.0f});
    renderer.DrawSphere(sphere_transform, default_material, 2.0f, 2.0f, 32, 32);

    // Helmet.
    const Rndr::Matrix4x4f helmet_transform = Opal::Translate(Rndr::Vector3f{0.0f, 2.0f, -20.0f}) * Opal::RotateX(90.0f);
    renderer.DrawModel("DamagedHelmet", helmet_model, helmet_transform);

    // Grid of white spheres.
    const Rndr::Canvas::PbrMaterialDesc white_material{.material_name = "White Material", .albedo_color = Rndr::Colors::k_white};
    constexpr Rndr::i32 k_cube_size = 10;
    const Rndr::Point3f start_position = {0.0f, 0.0f, 10.0f};
    for (Rndr::i32 x = 0; x < k_cube_size; ++x)
    {
        for (Rndr::i32 y = 0; y < k_cube_size; ++y)
        {
            for (Rndr::i32 z = 0; z < k_cube_size; ++z)
            {
                constexpr Rndr::f32 k_distance = 5.0f;
                const Rndr::Point3f draw_location = start_position + Rndr::Vector3f{x * k_distance, y * k_distance, z * k_distance};
                renderer.DrawSphere(Opal::Translate(draw_location), white_material);
            }
        }
    }
}
