#include "rndr/rndr.h"

void Run();

/**
 * In this example you will learn how to:
 *      1. Update a data buffer per frame.
 *      2. Render a mesh using just vertices, with no index buffers.
 *      3. Render wireframes.
 *      4. Use math transformations.
 */
int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
}

const char* const g_shader_code_vertex = R"(
        #version 460 core
        layout(std140, binding = 0) uniform PerFrameData {
          uniform mat4 MVP;
          uniform int isWireframe;
        };
        layout (location=0) out vec3 color;
        const vec3 pos[8] = vec3[8](
          vec3(-1.0,-1.0, 1.0), vec3( 1.0,-1.0, 1.0),
          vec3(1.0, 1.0, 1.0),  vec3(-1.0, 1.0, 1.0),
          vec3(-1.0,-1.0,-1.0), vec3(1.0,-1.0,-1.0),
          vec3( 1.0, 1.0,-1.0), vec3(-1.0, 1.0,-1.0)
        );
        const vec3 col[8] = vec3[8](
          vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
          vec3(0.0, 0.0, 1.0), vec3(1.0, 1.0, 0.0),
          vec3(1.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0),
          vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0)
        );
        const int indices[36] = int[36](
          0, 1, 2, 2, 3, 0, // front
          1, 5, 6, 6, 2, 1, // right
          7, 6, 5, 5, 4, 7, // back
          4, 0, 3, 3, 7, 4, // left
          4, 5, 1, 1, 0, 4, // bottom
          3, 2, 6, 6, 7, 3  // top
        );
        void main() {
          int idx = indices[gl_VertexID];
          gl_Position = MVP * vec4(pos[idx], 1.0);
          color = isWireframe > 0 ? vec3(0.0) : col[idx];
        }
    )";

const char* const g_shader_code_fragment = R"(
        #version 460 core
        layout (location=0) in vec3 color;
        layout (location=0) out vec4 out_FragColor;
        void main() {
          out_FragColor = vec4(color, 1.0);
        };
    )";

struct PerFrameData
{
    Rndr::Matrix4x4f mvp;
    int is_wire_frame;
};
constexpr size_t k_per_frame_size = sizeof(PerFrameData);

void Run()
{
    Rndr::Window window({.width = 800, .height = 600, .name = "Cube Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight()});
    RNDR_ASSERT(swap_chain.IsValid());
    Rndr::Shader vertex_shader(graphics_context, {.type = Rndr::ShaderType::Vertex, .source = g_shader_code_vertex});
    RNDR_ASSERT(vertex_shader.IsValid());
    Rndr::Shader pixel_shader(graphics_context, {.type = Rndr::ShaderType::Fragment, .source = g_shader_code_fragment});
    RNDR_ASSERT(pixel_shader.IsValid());
    const Rndr::Pipeline solid_pipeline(graphics_context, {.vertex_shader = &vertex_shader,
                                                           .pixel_shader = &pixel_shader,
                                                           .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                           .depth_stencil = {.is_depth_enabled = true}});
    RNDR_ASSERT(solid_pipeline.IsValid());
    const Rndr::Pipeline wireframe_pipeline(
        graphics_context, {.vertex_shader = &vertex_shader,
                           .pixel_shader = &pixel_shader,
                           .rasterizer = {.fill_mode = Rndr::FillMode::Wireframe, .depth_bias = -1.0, .slope_scaled_depth_bias = -1.0},
                           .depth_stencil = {.is_depth_enabled = true}});
    RNDR_ASSERT(wireframe_pipeline.IsValid());
    Rndr::Buffer per_frame_buffer(
        graphics_context,
        {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
    constexpr Rndr::Vector4f k_clear_color = Rndr::Colors::k_black;

    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height) { swap_chain.SetSize(width, height); });

    while (!window.IsClosed())
    {
        window.ProcessEvents();

        const float ratio = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
        const float angle = static_cast<float>(std::fmod(10 * Rndr::GetSystemTime(), 360.0));
        const Rndr::Matrix4x4f t =
            Math::Translate(Rndr::Vector3f(0.0f, 0.0f, -3.5f)) * Math::Rotate(angle, Rndr::Vector3f(1.0f, 1.0f, 1.0f));
        const Rndr::Matrix4x4f p = Math::Perspective_RH_N1(45.0f, ratio, 0.1f, 1000.0f);
        Rndr::Matrix4x4f mvp = p * t;
        mvp = Math::Transpose(mvp);
        PerFrameData per_frame_data = {.mvp = mvp, .is_wire_frame = 0};

        graphics_context.Update(per_frame_buffer, Rndr::ToByteSpan(per_frame_data));

        constexpr int32_t k_index_count = 36;
        graphics_context.ClearColorAndDepth(k_clear_color);
        graphics_context.Bind(swap_chain);
        graphics_context.Bind(solid_pipeline);
        graphics_context.Bind(per_frame_buffer, 0);
        graphics_context.DrawVertices(Rndr::PrimitiveTopology::Triangle, k_index_count);

        graphics_context.Bind(wireframe_pipeline);
        per_frame_data.is_wire_frame = 1;
        graphics_context.Update(per_frame_buffer, Rndr::ToByteSpan(per_frame_data));
        graphics_context.DrawVertices(Rndr::PrimitiveTopology::Triangle, k_index_count);

        graphics_context.Present(swap_chain);
    }
}