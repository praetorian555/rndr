#include "../../build/opengl-msvc-opt-debug/_deps/opal-src/include/opal/paths.h"
#include "opal/math/transform.h"
#include "opal/time.h"

#include "rndr/application.hpp"
#include "rndr/file.h"
#include "rndr/fly-camera.h"
#include "rndr/log.h"
#include "rndr/projections.h"
#include "rndr/render-api.h"
#include "rndr/types.h"

struct RNDR_ALIGN(16) Uniforms
{
    Rndr::Matrix4x4f view;
    Rndr::Matrix4x4f projection;
    Rndr::Vector3f position;
};

void SetupFlyCameraControls(Rndr::Application& app, Rndr::FlyCamera& camera);

int main()
{
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
    Rndr::i32 width = 0;
    Rndr::i32 height = 0;
    Rndr::i32 x = 0;
    Rndr::i32 y = 0;
    window->GetPositionAndSize(x, y, width, height);
    window->SetTitle("Window Sample");

    app->EnableHighPrecisionCursorMode(true, *window);

    const Rndr::GraphicsContextDesc gc_desc{.window_handle = window->GetNativeHandle()};
    Rndr::GraphicsContext gc{gc_desc};
    const Rndr::SwapChainDesc swap_chain_desc{.width = width, .height = height};
    Rndr::SwapChain swap_chain{gc, swap_chain_desc};

    app->on_window_resize.Bind(
        [&swap_chain, window](const Rndr::GenericWindow& w, Rndr::i32 width, Rndr::i32 height)
        {
            if (window == &w)
            {
                swap_chain.SetSize(width, height);
            }
        });

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

    const Rndr::FlyCameraDesc fly_camera_desc;
    Rndr::FlyCamera camera(width, height, fly_camera_desc);
    SetupFlyCameraControls(*app, camera);
    app->on_window_resize.Bind(
        [&camera, window](const Rndr::GenericWindow& w, Rndr::i32 width, Rndr::i32 height)
        {
            if (window == &w)
            {
                camera.SetScreenSize(width, height);
            }
        });

    Rndr::f64 check_change_time = 0.0;
    Rndr::f32 delta_seconds = 0.016f;
    while (!window->IsClosed())
    {
        const Rndr::f64 start_seconds = Opal::GetSeconds();

        app->ProcessSystemEvents(delta_seconds);

        camera.Tick(delta_seconds);

        gc.BindSwapChainFrameBuffer(swap_chain);
        gc.ClearAll(Rndr::Colors::k_black);

        Uniforms uniforms;
        uniforms.view = Opal::Transpose(camera.FromWorldToCamera());
        uniforms.projection = Opal::Transpose(camera.FromCameraToNDC());
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

        gc.Present(swap_chain);

        const Rndr::f64 end_seconds = Opal::GetSeconds();
        delta_seconds = static_cast<Rndr::f32>(end_seconds - start_seconds);
    }

    uniform_buffer.Destroy();
    pipeline.Destroy();
    vertex_shader.Destroy();
    fragment_shader.Destroy();
    gc.Destroy();
    app->DestroyGenericWindow(window);
    Rndr::Application::Destroy();
    return 0;
}

static Rndr::FlyCamera* g_camera;
static Rndr::f32 g_camera_rotation_yaw_speed = 0.005f;
static Rndr::f32 g_camera_rotation_roll_speed = 0.002f;
static Rndr::f32 g_camera_move_speed = 0.5f;
static Rndr::InputContext g_camera_movement_context("Camera Movement Context");

void HandleLookVertical(Rndr::InputPrimitive, Rndr::f32 axis_value);
void HandleLookVerticalButton(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool);
void HandleLookHorizontal(Rndr::InputPrimitive, Rndr::f32 axis_value);
void HandleLookHorizontalButton(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool);
void HandleMoveForward(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool);
void HandleMoveRight(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool);

void SetupFlyCameraControls(Rndr::Application& app, Rndr::FlyCamera& camera)
{
    g_camera = &camera;

    using IB = Rndr::InputBinding;
    using IT = Rndr::InputTrigger;
    using IP = Rndr::InputPrimitive;

    Rndr::InputSystem& input_system = app.GetInputSystemChecked();
    g_camera_movement_context.SetEnabled(false);
    input_system.GetCurrentContext().AddAction(
        "Toggle movement controls",
        {
            Rndr::InputBinding::CreateKeyboardButtonBinding(
                Rndr::InputPrimitive::F1, Rndr::InputTrigger::ButtonPressed, [](Rndr::InputPrimitive, Rndr::InputTrigger, Rndr::f32, bool is_repeated)
                {
                    if (!is_repeated)
                    {
                        g_camera_movement_context.SetEnabled(!g_camera_movement_context.IsEnabled());
                    }
                })
        });

    Opal::DynamicArray<IB> forward_bindings;
    forward_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::W, IT::ButtonPressed, HandleMoveForward));
    forward_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::W, IT::ButtonReleased, HandleMoveForward));
    forward_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::S, IT::ButtonPressed, HandleMoveForward, -1));
    forward_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::S, IT::ButtonReleased, HandleMoveForward));
    g_camera_movement_context.AddAction("MoveForward", forward_bindings);

    Opal::DynamicArray<IB> right_bindings;
    right_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::A, IT::ButtonPressed, HandleMoveRight, -1));
    right_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::A, IT::ButtonReleased, HandleMoveRight));
    right_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::D, IT::ButtonPressed, HandleMoveRight));
    right_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::D, IT::ButtonReleased, HandleMoveRight));
    g_camera_movement_context.AddAction("MoveRight", right_bindings);

    Opal::DynamicArray<IB> vert_bindings;
    vert_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::UpArrow, IT::ButtonPressed, HandleLookVerticalButton));
    vert_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::UpArrow, IT::ButtonReleased, HandleLookVerticalButton));
    vert_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::DownArrow, IT::ButtonPressed, HandleLookVerticalButton, -1));
    vert_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::DownArrow, IT::ButtonReleased, HandleLookVerticalButton));
    vert_bindings.PushBack(IB::CreateMousePositionBinding(IP::Mouse_AxisY, HandleLookVertical));
    g_camera_movement_context.AddAction("LookAroundVert", vert_bindings);

    Opal::DynamicArray<IB> horz_bindings;
    horz_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::RightArrow, IT::ButtonPressed, HandleLookHorizontalButton, -1));
    horz_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::RightArrow, IT::ButtonReleased, HandleLookHorizontalButton));
    horz_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::LeftArrow, IT::ButtonPressed, HandleLookHorizontalButton));
    horz_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::LeftArrow, IT::ButtonReleased, HandleLookHorizontalButton));
    horz_bindings.PushBack(IB::CreateMousePositionBinding(IP::Mouse_AxisX, HandleLookHorizontal));
    g_camera_movement_context.AddAction("LookAroundHorz", horz_bindings);

    app.GetInputSystemChecked().PushContext(Opal::Ref(&g_camera_movement_context));
}

void HandleLookVertical(Rndr::InputPrimitive, Rndr::f32 axis_value)
{
    g_camera->AddPitch(-g_camera_rotation_roll_speed * axis_value);
}

void HandleLookVerticalButton(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool)
{
    if (trigger == Rndr::InputTrigger::ButtonPressed)
    {
        g_camera->AddPitch(g_camera_rotation_roll_speed * value);
    }
}

void HandleLookHorizontal(Rndr::InputPrimitive, Rndr::f32 axis_value)
{
    g_camera->AddYaw(-g_camera_rotation_yaw_speed * axis_value);
}

void HandleLookHorizontalButton(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool)
{
    if (trigger == Rndr::InputTrigger::ButtonPressed)
    {
        g_camera->AddYaw(g_camera_rotation_yaw_speed * value);
    }
}

void HandleMoveForward(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool)
{
    if (trigger == Rndr::InputTrigger::ButtonPressed)
    {
        g_camera->MoveForward(g_camera_move_speed * value);
    }
}

void HandleMoveRight(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool)
{
    if (trigger == Rndr::InputTrigger::ButtonPressed)
    {
        g_camera->MoveRight(g_camera_move_speed * value);
    }
}
