#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/application.hpp"
#include "rndr/file.h"
#include "rndr/fly-camera.h"
#include "rndr/log.h"
#include "rndr/render-api.h"
#include "rndr/types.h"

#include "example-controller.h"

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

    const Rndr::FlyCameraDesc fly_camera_desc{ .start_yaw_radians = Opal::k_pi_over_2_float };
    ExampleController controller(*app, width, height, fly_camera_desc, 10.0f, 0.5f, 0.2f);

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
                                        [&controller](Rndr::InputPrimitive, Rndr::InputTrigger, Rndr::f32, bool is_repeated)
                                        {
                                            if (!is_repeated)
                                            {
                                                controller.Enable(!controller.IsEnabled());
                                            }
                                        })});

    Rndr::f64 check_change_time = 0.0;
    Rndr::f32 delta_seconds = 0.016f;
    while (!window->IsClosed())
    {
        const Rndr::f64 start_seconds = Opal::GetSeconds();

        app->ProcessSystemEvents(delta_seconds);

        controller.Tick(delta_seconds);

        gc.BindSwapChainFrameBuffer(swap_chain);
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
