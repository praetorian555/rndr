#include "rndr/rndr.h"

void Run();

/**
 * Basic example on how to use the Rndr library. This example creates a window, initializes a
 * graphics context and a swap chain, sets up a simple vertex and pixel shader, and a pipeline state
 * object. It then draws a triangle to the screen.
 */
int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
}

void Run()
{
    Rndr::Window window({.width = 800, .height = 600, .name = "Triangle"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    assert(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context,
                               {.width = window.GetWidth(), .height = window.GetHeight()});
    assert(swap_chain.IsValid());
    Rndr::Shader vertex_shader(graphics_context, {.type = Rndr::ShaderType::Vertex, .source = R"(
        #version 460 core
        layout (location=0) out vec3 color;
        const vec2 pos[3] = vec2[3](
          vec2(-0.6, -0.4),
          vec2(0.6, -0.4),
          vec2(0.0, 0.6)
        );
        const vec3 col[3] = vec3[3](
          vec3(1.0, 0.0, 0.0),
          vec3(0.0, 1.0, 0.0),
          vec3(0.0, 0.0, 1.0)
        );
        void main() {
          gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
          color = col[gl_VertexID];
        }
        )"});
    assert(vertex_shader.IsValid());
    Rndr::Shader pixel_shader(graphics_context, {.type = Rndr::ShaderType::Fragment, .source = R"(
        #version 460 core
        layout (location=0) in vec3 color;
        layout (location=0) out vec4 out_FragColor;
        void main() {
          out_FragColor = vec4(color, 1.0);
        };
        )"});
    assert(pixel_shader.IsValid());
    const Rndr::Pipeline pipeline(graphics_context,
                                  {.vertex_shader = &vertex_shader, .pixel_shader = &pixel_shader});
    assert(pipeline.IsValid());
    constexpr math::Vector4 k_clear_color{MATH_REALC(0.0),
                                          MATH_REALC(0.0),
                                          MATH_REALC(0.0),
                                          MATH_REALC(1.0)};

    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height)
                          { swap_chain.SetSize(width, height); });

    while (!window.IsClosed())
    {
        window.ProcessEvents();

        graphics_context.Bind(swap_chain);
        graphics_context.Bind(pipeline);
        graphics_context.ClearColor(k_clear_color);
        graphics_context.DrawVertices(Rndr::PrimitiveTopology::Triangle, 3);

        graphics_context.Present(swap_chain, true);
    }
}