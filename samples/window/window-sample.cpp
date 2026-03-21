#include "opal/paths.h"
#include "opal/time.h"

#include "../../include/rndr/canvas/renderers/pbr-renderer.hpp"
#include "rndr/application.hpp"
#include "rndr/bitmap.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/draw-list.hpp"
#include "rndr/canvas/projections.hpp"
#include "rndr/canvas/texture.hpp"
#include "rndr/file.hpp"
#include "rndr/fly-camera.hpp"
#include "rndr/frames-per-second-counter.hpp"
#include "rndr/imgui-system.hpp"
#include "rndr/log.hpp"
#include "rndr/types.hpp"

#include "example-controller.h"
#include "imgui.h"
#include "opal/rng.h"

static Rndr::Canvas::Texture LoadTextureFromFile(const Rndr::Canvas::Context& context, const Opal::StringUtf8& path)
{
    const Rndr::Bitmap bitmap = Rndr::File::LoadImage(path, true, true);
    RNDR_ASSERT(bitmap.IsValid(), "Failed to load image!");

    Rndr::Canvas::Format format = Rndr::Canvas::Format::RGBA8;
    switch (bitmap.GetComponentCount())
    {
        case 1:
            format = Rndr::Canvas::Format::R8;
            break;
        case 2:
            format = Rndr::Canvas::Format::RG8;
            break;
        case 3:
            format = Rndr::Canvas::Format::RGB8;
            break;
        case 4:
            format = Rndr::Canvas::Format::RGBA8;
            break;
    }

    const Rndr::Canvas::TextureDesc desc{
        .width = bitmap.GetWidth(),
        .height = bitmap.GetHeight(),
        .format = format,
        .wrap_u = Rndr::Canvas::TextureWrap::Repeat,
        .wrap_v = Rndr::Canvas::TextureWrap::Repeat,
        .use_mips = bitmap.GetMipCount() > 1,
    };
    const Opal::ArrayView<const Rndr::u8> data(bitmap.GetData(), bitmap.GetTotalSize());
    return Rndr::Canvas::Texture(context, desc, data, path.Clone());
}

void SetupFlyCameraControls(Rndr::Application& app, Rndr::FlyCamera& camera);

void DrawScene(Rndr::Canvas::PbrRenderer& renderer, const Rndr::Mesh& helmet_mesh, const Rndr::Canvas::Texture* default_albedo_texture,
               const Rndr::Canvas::Texture* helmet_albedo, const Rndr::Canvas::Texture* helmet_emissive,
               const Rndr::Canvas::Texture* helmet_mr, const Rndr::Canvas::Texture* helmet_normal, const Rndr::Canvas::Texture* helmet_ao);

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

    auto context = Canvas::Context::Init(window.Clone());
    RNDR_ASSERT(context.IsValid(), "Failed to create Canvas context!");

    Rndr::ImGuiContext imgui_context(*app, window.Clone());

    Canvas::PbrRenderer pbr_renderer(Opal::Ref{context});

    // Load helmet mesh.
    Mesh helmet_mesh;
    MaterialDesc helmet_material_desc;
    const Opal::StringUtf8 helmet_path =
        Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "DamagedHelmet", "gltf", "DamagedHelmet.gltf");
    File::LoadMeshAndMaterialDescription(helmet_path, helmet_mesh, helmet_material_desc);

    // Load textures.
    Opal::StringUtf8 default_albedo_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "default-texture.png");
    default_albedo_path = Opal::Paths::NormalizePath(std::move(default_albedo_path));
    Canvas::Texture default_albedo_texture = LoadTextureFromFile(context, default_albedo_path);

    Canvas::Texture helmet_albedo;
    if (!helmet_material_desc.albedo_texture_path.IsEmpty())
    {
        helmet_albedo = LoadTextureFromFile(context, helmet_material_desc.albedo_texture_path);
    }
    Canvas::Texture helmet_emissive;
    if (!helmet_material_desc.emissive_texture_path.IsEmpty())
    {
        helmet_emissive = LoadTextureFromFile(context, helmet_material_desc.emissive_texture_path);
    }
    Canvas::Texture helmet_mr;
    if (!helmet_material_desc.metallic_roughness_texture_path.IsEmpty())
    {
        helmet_mr = LoadTextureFromFile(context, helmet_material_desc.metallic_roughness_texture_path);
    }
    Canvas::Texture helmet_normal;
    if (!helmet_material_desc.normal_texture_path.IsEmpty())
    {
        helmet_normal = LoadTextureFromFile(context, helmet_material_desc.normal_texture_path);
    }
    Canvas::Texture helmet_ao;
    if (!helmet_material_desc.ambient_occlusion_texture_path.IsEmpty())
    {
        helmet_ao = LoadTextureFromFile(context, helmet_material_desc.ambient_occlusion_texture_path);
    }

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
        .GetCurrentContext()
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

    FramesPerSecondCounter fps_counter;
    bool stats_window = true;
    f32 delta_seconds = 0.016f;
    while (!window->IsClosed())
    {
        const f64 start_seconds = Opal::GetSeconds();

        fps_counter.Update(delta_seconds);

        app->ProcessSystemEvents(delta_seconds);
        controller.Tick(delta_seconds);

        Canvas::DrawList draw_list;
        draw_list.SetRenderTarget(context);
        draw_list.Clear(Colors::k_black, 1.0f);

        const Matrix4x4f vp = controller.GetProjectionTransform() * controller.GetViewTransform();

        pbr_renderer.BeginFrame();
        pbr_renderer.SetViewProjection(vp);
        pbr_renderer.SetCameraPosition(controller.GetCameraPosition());

        DrawScene(pbr_renderer, helmet_mesh, &default_albedo_texture, helmet_albedo.IsValid() ? &helmet_albedo : nullptr,
                  helmet_emissive.IsValid() ? &helmet_emissive : nullptr, helmet_mr.IsValid() ? &helmet_mr : nullptr,
                  helmet_normal.IsValid() ? &helmet_normal : nullptr, helmet_ao.IsValid() ? &helmet_ao : nullptr);

        pbr_renderer.Render(draw_list);
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
        ImGui::End();
        imgui_context.EndFrame();

        context.Present();

        const f64 end_seconds = Opal::GetSeconds();
        delta_seconds = static_cast<f32>(end_seconds - start_seconds);
    }

    return 0;
}

void DrawScene(Rndr::Canvas::PbrRenderer& renderer, const Rndr::Mesh& helmet_mesh, const Rndr::Canvas::Texture* default_albedo_texture,
               const Rndr::Canvas::Texture* helmet_albedo, const Rndr::Canvas::Texture* helmet_emissive,
               const Rndr::Canvas::Texture* helmet_mr, const Rndr::Canvas::Texture* helmet_normal, const Rndr::Canvas::Texture* helmet_ao)
{
    renderer.AddDirectionalLight({1, 1, 1}, Rndr::Colors::k_white);
    renderer.AddPointLight({-20, 0, 0}, Rndr::Colors::k_red);

    // Red cube.
    const Rndr::Canvas::PbrMaterialDesc red_material{.material_name = "Green Material", .albedo_color = Rndr::Colors::k_green};
    const Rndr::Matrix4x4f cube_transform = Opal::Translate(Rndr::Vector3f{-2.0f, 0.0f, -10.0f});
    renderer.DrawCube(cube_transform, red_material);

    // Textured sphere.
    const Rndr::Canvas::PbrMaterialDesc default_material{.material_name = "Default Material", .albedo_texture = default_albedo_texture};
    const Rndr::Matrix4x4f sphere_transform = Opal::Translate(Rndr::Vector3f{2.0f, 0.0f, -10.0f});
    renderer.DrawSphere(sphere_transform, default_material, 2.0f, 2.0f, 32, 32);

    // Helmet.
    Rndr::Canvas::PbrMaterialDesc helmet_material;
    helmet_material.material_name = "Damaged Helmet Material";
    helmet_material.albedo_texture = helmet_albedo;
    helmet_material.emissive_texture = helmet_emissive;
    helmet_material.metallic_roughness_texture = helmet_mr;
    helmet_material.normal_texture = helmet_normal;
    helmet_material.ambient_occlusion_texture = helmet_ao;
    const Rndr::Matrix4x4f helmet_transform = Opal::Translate(Rndr::Vector3f{0.0f, 2.0f, -20.0f}) * Opal::RotateX(90.0f);
    renderer.DrawMesh("DamagedHelmet", helmet_mesh, helmet_transform, helmet_material);

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
