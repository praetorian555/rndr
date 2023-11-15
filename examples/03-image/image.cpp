#include "rndr/rndr.h"

#include <thread>

#if RNDR_ETC2COMP
#include "EtcLib/Etc/Etc.h"
#include "EtcLib/Etc/EtcImage.h"
#include "EtcTool/EtcFile.h"
#endif

void Run();

/**
 * In this example you will learn how to:
 *    1. Load a texture from a file.
 *    2. Render a textured triangle.
 *    3. Use the input system.
 *    4. Save image to a file.
 *    5. Compress image using ETC2.
 *    6. Use ImGui.
 */
int main()
{
    Rndr::Init({.enable_input_system = true});
    Run();
    Rndr::Destroy();
}

static const char* const g_vertex_shader_source = R"(
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

static const char* const g_pixel_shader_source = R"(
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
    Rndr::Matrix4x4f mvp;
};

void Run()
{
    Rndr::Window window({.width = 800, .height = 600, .name = "Image Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight()});
    RNDR_ASSERT(swap_chain.IsValid());
    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height) { swap_chain.SetSize(width, height); });

    Rndr::Shader vertex_shader(graphics_context, {.type = Rndr::ShaderType::Vertex, .source = g_vertex_shader_source});
    RNDR_ASSERT(vertex_shader.IsValid());
    Rndr::Shader pixel_shader(graphics_context, {.type = Rndr::ShaderType::Fragment, .source = g_pixel_shader_source});
    RNDR_ASSERT(pixel_shader.IsValid());
    const Rndr::Pipeline pipeline(graphics_context, {.vertex_shader = &vertex_shader, .pixel_shader = &pixel_shader});
    RNDR_ASSERT(pipeline.IsValid());

    constexpr int32_t k_per_frame_size = sizeof(PerFrameData);
    Rndr::Buffer per_frame_buffer(
        graphics_context,
        {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
    RNDR_ASSERT(per_frame_buffer.IsValid());

    Rndr::Bitmap bitmap = Rndr::File::ReadEntireImage(ASSETS_DIR "brick-wall.jpg", Rndr::PixelFormat::R8G8B8_UNORM_SRGB);
    RNDR_ASSERT(bitmap.IsValid());
    constexpr bool k_use_mips = false;
    const Rndr::Image image(graphics_context, bitmap, k_use_mips, {});
    RNDR_ASSERT(image.IsValid());

    constexpr Rndr::Vector4f k_clear_color = Rndr::Colors::k_white;

    Rndr::InputContext& input_ctx = Rndr::InputSystem::GetCurrentContext();
    const Rndr::InputAction exit_action{"exit"};
    Rndr::InputActionData exit_action_data;
    exit_action_data.callback = [&window](Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, float value)
    {
        RNDR_UNUSED(primitive);
        RNDR_UNUSED(trigger);
        RNDR_UNUSED(value);
        window.Close();
    };
    exit_action_data.native_window = window.GetNativeWindowHandle();
    input_ctx.AddAction(exit_action, exit_action_data);
    input_ctx.AddBindingToAction(
        exit_action, Rndr::InputBinding{.primitive = Rndr::InputPrimitive::Keyboard_Esc, .trigger = Rndr::InputTrigger::ButtonReleased});

    const Rndr::InputAction screenshot_action{"screenshot"};
    Rndr::InputActionData screenshot_action_data;
    screenshot_action_data.callback =
        [&graphics_context, &swap_chain](Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, float value)
    {
        RNDR_UNUSED(primitive);
        RNDR_UNUSED(trigger);
        RNDR_UNUSED(value);
        Rndr::Bitmap image_to_save;
        const bool is_read = graphics_context.ReadSwapChainColor(swap_chain, image_to_save);
        RNDR_ASSERT(is_read);
        RNDR_UNUSED(is_read);
        Rndr::File::SaveImage(image_to_save, "screenshot.png");

#if RNDR_ETC2COMP
        Rndr::Array<float> image_to_save_float(image_to_save.GetSize2D());
        for (size_t i = 0; i < image_to_save.GetSize2D(); ++i)
        {
            image_to_save_float[i] = static_cast<float>(image_to_save.GetData()[i]) / 255.0f;
        }

        const Etc::Image::Format etc_format = Etc::Image::Format::RGB8;
        const auto error_metric = Etc::ErrorMetric::BT709;
        Etc::Image image(image_to_save_float.data(), image_to_save.GetWidth(), image_to_save.GetHeight(), error_metric);

        image.Encode(etc_format, error_metric, ETCCOMP_DEFAULT_EFFORT_LEVEL, std::thread::hardware_concurrency(), 1024);

        Etc::File etc_file("screenshot.ktx", Etc::File::Format::KTX, etc_format, image.GetEncodingBits(), image.GetEncodingBitsBytes(),
                           image.GetSourceWidth(), image.GetSourceHeight(), image.GetExtendedWidth(), image.GetExtendedHeight());
        etc_file.Write();
#endif
    };
    screenshot_action_data.native_window = window.GetNativeWindowHandle();
    input_ctx.AddAction(screenshot_action, screenshot_action_data);
    input_ctx.AddBindingToAction(screenshot_action, Rndr::InputBinding{.primitive = Rndr::InputPrimitive::Keyboard_F9,
                                                                       .trigger = Rndr::InputTrigger::ButtonReleased});

    Rndr::ImGuiWrapper::Init(window, graphics_context, {.display_demo_window = true});

    while (!window.IsClosed())
    {
        window.ProcessEvents();
        Rndr::InputSystem::ProcessEvents(0);

        const float ratio = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
        const float angle = static_cast<float>(std::fmod(10 * Rndr::GetSystemTime(), 360.0));
        const Rndr::Matrix4x4f t = Math::Rotate(angle, Rndr::Vector3f(0.0f, 0.0f, 1.0f));
        const Rndr::Matrix4x4f p = Math::Orthographic_RH_N1(-ratio, ratio, -1.0f, 1.0f, -1.0f, 1.0f);
        Rndr::Matrix4x4f mvp = p * t;
        mvp = Math::Transpose(mvp);
        PerFrameData per_frame_data = {.mvp = mvp};

        graphics_context.Update(per_frame_buffer, Rndr::ToByteSpan(per_frame_data));

        graphics_context.Bind(swap_chain);
        graphics_context.Bind(pipeline);
        graphics_context.Bind(per_frame_buffer, 0);
        graphics_context.Bind(image, 0);
        graphics_context.ClearColor(k_clear_color);
        graphics_context.DrawVertices(Rndr::PrimitiveTopology::Triangle, 3);

        Rndr::ImGuiWrapper::StartFrame();
        Rndr::ImGuiWrapper::EndFrame();

        graphics_context.Present(swap_chain);
    }

    Rndr::ImGuiWrapper::Destroy();
}