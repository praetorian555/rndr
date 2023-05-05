#include "rndr/rndr.h"

#include <thread>

#if RNDR_ETC2COMP
#include "EtcLib/Etc/Etc.h"
#include "EtcLib/Etc/EtcFilter.h"
#include "EtcLib/Etc/EtcImage.h"
#include "EtcTool/EtcFile.h"
#endif

void Run();

/**
 * Shows how to use a texture which is called image in this library. It also shows how to use
 * the input system.
 */
int main()
{
    Rndr::Init({.enable_input_system = true});
    Run();
    Rndr::Destroy();
}

static const char* g_vertex_shader_source = R"(
#version 460 core
layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 MVP;
};
layout (location=0) out vec2 uv;
const vec2 pos[3] = vec2[3](
	vec2(-0.6f, -0.4f),
	vec2( 0.6f, -0.4f),
	vec2( 0.0f,  0.6f)
);
const vec2 tc[3] = vec2[3](
	vec2( 0.0, 0.0 ),
	vec2( 1.0, 0.0 ),
	vec2( 0.5, 1.0 )
);
void main()
{
	gl_Position = MVP * vec4(pos[gl_VertexID], 0.0, 1.0);
	uv = tc[gl_VertexID];
}
)";

static const char* g_pixel_shader_source = R"(
#version 460 core
layout (location=0) in vec2 uv;
layout (location=0) out vec4 out_FragColor;
uniform sampler2D texture0;
void main()
{
	out_FragColor = texture(texture0, uv);
};
)";

struct PerFrameData
{
    math::Matrix4x4 mvp;
};

void Run()
{
    Rndr::Window window({.width = 800, .height = 600, .name = "Triangle"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    assert(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context,
                               {.width = window.GetWidth(), .height = window.GetHeight()});
    assert(swap_chain.IsValid());
    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height)
                          { swap_chain.SetSize(width, height); });

    Rndr::Shader vertex_shader(
        graphics_context,
        {.type = Rndr::ShaderType::Vertex, .source = g_vertex_shader_source});
    assert(vertex_shader.IsValid());
    Rndr::Shader pixel_shader(
        graphics_context,
        {.type = Rndr::ShaderType::Fragment, .source = g_pixel_shader_source});
    assert(pixel_shader.IsValid());
    const Rndr::Pipeline pipeline(graphics_context,
                                  {.vertex_shader = &vertex_shader, .pixel_shader = &pixel_shader});
    assert(pipeline.IsValid());

    constexpr int32_t k_per_frame_size = sizeof(PerFrameData);
    Rndr::Buffer per_frame_buffer(graphics_context,
                                  {.type = Rndr::BufferType::Constant,
                                   .usage = Rndr::Usage::Dynamic,
                                   .size = k_per_frame_size,
                                   .stride = k_per_frame_size});
    assert(per_frame_buffer.IsValid());

    Rndr::CPUImage cpu_image = Rndr::File::ReadEntireImage(IMAGE_DIR "/ch2_sample3_stb.jpg");
    assert(cpu_image.IsValid());
    constexpr bool k_use_mips = false;
    Rndr::Image image(graphics_context, cpu_image, k_use_mips, {});
    assert(image.IsValid());

    constexpr math::Vector4 k_clear_color{MATH_REALC(0.0),
                                          MATH_REALC(0.0),
                                          MATH_REALC(0.0),
                                          MATH_REALC(1.0)};

    Rndr::InputContext& input_ctx = Rndr::InputSystem::GetCurrentContext();
    const Rndr::InputAction exit_action{"exit"};
    Rndr::InputActionData exit_action_data;
    exit_action_data.callback =
        [&window](Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, Rndr::real value)
    {
        RNDR_UNUSED(primitive);
        RNDR_UNUSED(trigger);
        RNDR_UNUSED(value);
        window.Close();
    };
    exit_action_data.native_window = window.GetNativeWindowHandle();
    input_ctx.AddAction(exit_action, exit_action_data);
    input_ctx.AddBindingToAction(exit_action,
                                 Rndr::InputBinding{.primitive = Rndr::InputPrimitive::Keyboard_Esc,
                                                    .trigger = Rndr::InputTrigger::ButtonReleased});

    const Rndr::InputAction screenshot_action{"screenshot"};
    Rndr::InputActionData screenshot_action_data;
    screenshot_action_data.callback =
        [&graphics_context,
         &swap_chain](Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, Rndr::real value)
    {
        RNDR_UNUSED(primitive);
        RNDR_UNUSED(trigger);
        RNDR_UNUSED(value);
        const Rndr::CPUImage image_to_save = graphics_context.ReadSwapChain(swap_chain);
        Rndr::File::SaveImage(image_to_save, "screenshot.png");

#if RNDR_ETC2COMP
        Rndr::Array<float> image_to_save_float(image_to_save.data.size());
        for (size_t i = 0; i < image_to_save.data.size(); ++i)
        {
            image_to_save_float[i] = static_cast<float>(image_to_save.data[i]) / 255.0f;
        }

        const Etc::Image::Format etc_format = Etc::Image::Format::RGB8;
        const auto error_metric = Etc::ErrorMetric::BT709;
        Etc::Image image(image_to_save_float.data(),
                         image_to_save.width,
                         image_to_save.height,
                         error_metric);

        image.Encode(etc_format,
                     error_metric,
                     ETCCOMP_DEFAULT_EFFORT_LEVEL,
                     std::thread::hardware_concurrency(),
                     1024);

        Etc::File etc_file("screenshot.ktx",
                          Etc::File::Format::KTX,
                          etc_format,
                          image.GetEncodingBits(),
                          image.GetEncodingBitsBytes(),
                          image.GetSourceWidth(),
                          image.GetSourceHeight(),
                          image.GetExtendedWidth(),
                          image.GetExtendedHeight());
        etc_file.Write();
#endif
    };
    screenshot_action_data.native_window = window.GetNativeWindowHandle();
    input_ctx.AddAction(screenshot_action, screenshot_action_data);
    input_ctx.AddBindingToAction(screenshot_action,
                                 Rndr::InputBinding{.primitive = Rndr::InputPrimitive::Keyboard_F9,
                                                    .trigger = Rndr::InputTrigger::ButtonReleased});

    Rndr::ImGuiWrapper::Init(window, graphics_context, {.display_demo_window = true});

    while (!window.IsClosed())
    {
        window.ProcessEvents();
        Rndr::InputSystem::ProcessEvents(0);

        const float ratio = static_cast<Rndr::real>(window.GetWidth())
                            / static_cast<Rndr::real>(window.GetHeight());
        const Rndr::real angle = std::fmod(10 * Rndr::GetSystemTime(), 360.0f);
        const math::Transform t = math::Rotate(angle, math::Vector3(0.0f, 0.0f, 1.0f));
        const math::Matrix4x4 p = math::Orthographic_RH_N1(-ratio, ratio, -1.0f, 1.0f, -1.0f, 1.0f);
        math::Matrix4x4 mvp = math::Multiply(p, t.GetMatrix());
        mvp = mvp.Transpose();
        PerFrameData per_frame_data = {.mvp = mvp};

        graphics_context.Update(per_frame_buffer, Rndr::ToByteSpan(per_frame_data));

        graphics_context.Bind(swap_chain);
        graphics_context.Bind(pipeline);
        graphics_context.BindUniform(per_frame_buffer, 0);
        graphics_context.Bind(image);
        graphics_context.ClearColor(k_clear_color);
        graphics_context.DrawVertices(Rndr::PrimitiveTopology::Triangle, 3);

        Rndr::ImGuiWrapper::StartFrame();
        Rndr::ImGuiWrapper::EndFrame();

        graphics_context.Present(swap_chain, true);
    }

    Rndr::ImGuiWrapper::Destroy();
}