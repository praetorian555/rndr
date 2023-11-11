#include "rndr/rndr.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>

#include "mesh.h"

void Run(const char* mesh_path);

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        RNDR_LOG_ERROR("No mesh file provided!");
        RNDR_HALT("No mesh file provided!");
        return 1;
    }

    Rndr::Init({.enable_input_system = true, .enable_cpu_tracer = true});
    Run(argv[1]);
    Rndr::Destroy();
}

const char* const g_shader_code_vertex = R"(
#version 460 core
layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 MVP;
    int is_wire_frame;
};
layout (location=0) in vec3 pos;
layout (location=0) out vec3 color;
void main()
{
	gl_Position = MVP * vec4(pos, 1.0);
    color = is_wire_frame > 0 ? vec3(0.0, 0.0, 0.0) : pos.xyz;
}
)";

const char* const g_shader_code_fragment = R"(
#version 460 core
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;
void main()
{
	out_FragColor = vec4(color, 1.0);
};
)";

struct PerFrameData
{
    Rndr::Matrix4x4f mvp;
    int is_wire_frame;
};

class MeshRenderer : public Rndr::RendererBase
{
    struct DrawElementsIndirectCommand
    {
        uint32_t index_count;
        uint32_t instance_count;
        uint32_t first_index;
        uint32_t base_vertex;
        uint32_t base_instance;
    };

public:
    MeshRenderer(const Rndr::String& name, const Rndr::RendererBaseDesc& desc, const Rndr::String& mesh_path) : Rndr::RendererBase(name, desc)
    {
        constexpr uint32_t k_ai_process_flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                                aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                                                aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                                aiProcess_GenUVCoords;

        const aiScene* scene = aiImportFile(mesh_path.c_str(), k_ai_process_flags);
        if (scene == nullptr || !scene->HasMeshes())
        {
            RNDR_LOG_ERROR("Failed to load mesh from file with error: %s", aiGetErrorString());
            RNDR_HALT("Invalid mesh file!");
            return;
        }

        const bool is_data_loaded = ReadMeshData(m_mesh_data, *scene, k_load_positions);
        if (!is_data_loaded)
        {
            RNDR_LOG_ERROR("Failed to load mesh data from file: %s", mesh_path.c_str());
            RNDR_HALT("Failed  to load mesh data!");
            return;
        }
        aiReleaseImport(scene);

        m_vertex_shader = Rndr::Shader(m_desc.graphics_context, {.type = Rndr::ShaderType::Vertex, .source = g_shader_code_vertex});
        RNDR_ASSERT(m_vertex_shader.IsValid());
        m_pixel_shader = Rndr::Shader(m_desc.graphics_context, {.type = Rndr::ShaderType::Fragment, .source = g_shader_code_fragment});
        RNDR_ASSERT(m_pixel_shader.IsValid());

        m_index_buffer = Rndr::Buffer(m_desc.graphics_context,
                                      {.type = Rndr::BufferType::Index,
                                       .usage = Rndr::Usage::Default,
                                       .size = static_cast<uint32_t>(m_mesh_data.index_buffer_data.size()),
                                       .stride = sizeof(uint32_t)},
                                      Rndr::ToByteSpan(m_mesh_data.index_buffer_data));
        RNDR_ASSERT(m_index_buffer.IsValid());

        constexpr size_t k_stride = sizeof(Rndr::Point3f);
        m_vertex_buffer = Rndr::Buffer(m_desc.graphics_context,
                                       {.type = Rndr::BufferType::Vertex,
                                        .usage = Rndr::Usage::Default,
                                        .size = static_cast<uint32_t>(m_mesh_data.vertex_buffer_data.size()),
                                        .stride = k_stride},
                                       Rndr::ToByteSpan(m_mesh_data.vertex_buffer_data));
        RNDR_ASSERT(m_vertex_buffer.IsValid());

        Rndr::InputLayoutBuilder builder;
        const Rndr::InputLayoutDesc input_layout_desc = builder.AddVertexBuffer(m_vertex_buffer, 0, Rndr::DataRepetition::PerVertex)
                                                            .AppendElement(0, Rndr::PixelFormat::R32G32B32_FLOAT)
                                                            .AddIndexBuffer(m_index_buffer)
                                                            .Build();

        m_pipeline = Rndr::Pipeline(m_desc.graphics_context, Rndr::PipelineDesc{.vertex_shader = &m_vertex_shader,
                                                                                .pixel_shader = &m_pixel_shader,
                                                                                .input_layout = input_layout_desc,
                                                                                .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                                                .depth_stencil = {.is_depth_enabled = true}});
        m_wireframe_pipeline = Rndr::Pipeline(
            m_desc.graphics_context,
            Rndr::PipelineDesc{.vertex_shader = &m_vertex_shader,
                               .pixel_shader = &m_pixel_shader,
                               .input_layout = input_layout_desc,
                               .rasterizer = {.fill_mode = Rndr::FillMode::Wireframe, .depth_bias = -1.0, .slope_scaled_depth_bias = -1.0},
                               .depth_stencil = {.is_depth_enabled = true}});
        RNDR_ASSERT(m_wireframe_pipeline.IsValid());

        constexpr size_t k_per_frame_size = sizeof(PerFrameData);
        m_per_frame_buffer = Rndr::Buffer(
            m_desc.graphics_context,
            {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
        RNDR_ASSERT(m_per_frame_buffer.IsValid());

        Rndr::Array<Rndr::DrawIndicesData> draw_commands;
        draw_commands.resize(m_mesh_data.meshes.size());
        for (int i = 0; i < draw_commands.size(); i++)
        {
            const MeshDescription& mesh_desc = m_mesh_data.meshes[i];
            draw_commands[i] = {.index_count = mesh_desc.GetLodIndicesCount(0),
                                .instance_count = 1,
                                .first_index = mesh_desc.index_offset,
                                .base_vertex = mesh_desc.vertex_offset,
                                .base_instance = 0};
        }
        const Rndr::Span<Rndr::DrawIndicesData> draw_commands_span(draw_commands);

        m_command_list = Rndr::CommandList(m_desc.graphics_context);
        m_command_list.Bind(*m_desc.swap_chain);
        m_command_list.Bind(m_pipeline);
        m_command_list.BindConstantBuffer(m_per_frame_buffer, 0);
        m_command_list.DrawIndicesMulti(m_pipeline, Rndr::PrimitiveTopology::Triangle, draw_commands_span);

        m_wireframe_command_list = Rndr::CommandList(m_desc.graphics_context);
        m_wireframe_command_list.Bind(m_wireframe_pipeline);
        m_wireframe_command_list.BindConstantBuffer(m_per_frame_buffer, 0);
        m_wireframe_command_list.DrawIndicesMulti(m_wireframe_pipeline, Rndr::PrimitiveTopology::Triangle, draw_commands_span);
    }

    bool Render() override
    {
        RNDR_TRACE_SCOPED(Mesh rendering);

        // Rotate the mesh
        const Rndr::Matrix4x4f t = Math::RotateX(-90.0f);
        Rndr::Matrix4x4f mvp = m_camera_transform * t;
        mvp = Math::Transpose(mvp);
        PerFrameData per_frame_data = {.mvp = mvp, .is_wire_frame = 0};
        m_desc.graphics_context->Update(m_per_frame_buffer, Rndr::ToByteSpan(per_frame_data));

        m_command_list.Submit();

        per_frame_data.is_wire_frame = 1;
        m_desc.graphics_context->Update(m_per_frame_buffer, Rndr::ToByteSpan(per_frame_data));
        m_wireframe_command_list.Submit();

        return true;
    }

    void SetCameraTransform(const Rndr::Matrix4x4f& transform) { m_camera_transform = transform; }

private:
    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_pixel_shader;
    Rndr::Buffer m_vertex_buffer;
    Rndr::Buffer m_index_buffer;
    Rndr::Pipeline m_pipeline;
    Rndr::Pipeline m_wireframe_pipeline;
    Rndr::Buffer m_per_frame_buffer;
    Rndr::CommandList m_command_list;
    Rndr::CommandList m_wireframe_command_list;

    MeshData m_mesh_data;
    Rndr::Matrix4x4f m_camera_transform;
};

void Run(const char* mesh_path)
{
    Rndr::Window window({.width = 1600, .height = 1200, .name = "Mesh Renderer Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight(), .enable_vsync = false});
    RNDR_ASSERT(swap_chain.IsValid());

    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height) { swap_chain.SetSize(width, height); });

    Rndr::Array<Rndr::InputBinding> exit_bindings;
    exit_bindings.push_back({Rndr::InputPrimitive::Keyboard_Esc, Rndr::InputTrigger::ButtonReleased});
    Rndr::InputSystem::GetCurrentContext().AddAction(
        Rndr::InputAction("Exit"),
        Rndr::InputActionData{.callback = [&window](Rndr::InputPrimitive, Rndr::InputTrigger, float) { window.Close(); },
                              .native_window = window.GetNativeWindowHandle(),
                              .bindings = exit_bindings});

    const Rndr::RendererBaseDesc renderer_desc = {.graphics_context = Rndr::Ref{graphics_context}, .swap_chain = Rndr::Ref{swap_chain}};

    constexpr Rndr::Vector4f k_clear_color = Rndr::Colors::k_white;
    const Rndr::ScopePtr<Rndr::RendererBase> clear_renderer =
        RNDR_MAKE_SCOPED(Rndr::ClearRenderer, "Clear the screen", renderer_desc, k_clear_color);
    const Rndr::ScopePtr<Rndr::RendererBase> present_renderer =
        RNDR_MAKE_SCOPED(Rndr::PresentRenderer, "Present the back buffer", renderer_desc);
    const Rndr::ScopePtr<MeshRenderer> mesh_renderer = RNDR_MAKE_SCOPED(MeshRenderer, "Render a mesh", renderer_desc, mesh_path);

    Rndr::FlyCamera fly_camera(&window, &Rndr::InputSystem::GetCurrentContext(),
                               {.start_position = Rndr::Point3f(0.0f, 500.0f, 0.0f),
                                .movement_speed = 100,
                                .rotation_speed = 200,
                                .projection_desc = {.near = 0.5f, .far = 5000.0f}});

    Rndr::RendererManager renderer_manager;
    renderer_manager.AddRenderer(clear_renderer.get());
    renderer_manager.AddRenderer(mesh_renderer.get());
    renderer_manager.AddRenderer(present_renderer.get());

    Rndr::FramesPerSecondCounter fps_counter(0.1f);
    float delta_seconds = 0.033f;
    while (!window.IsClosed())
    {
        RNDR_TRACE_SCOPED(Frame);

        const Rndr::Timestamp start_time = Rndr::GetTimestamp();

        fps_counter.Update(delta_seconds);

        window.ProcessEvents();
        Rndr::InputSystem::ProcessEvents(delta_seconds);

        fly_camera.Update(delta_seconds);
        mesh_renderer->SetCameraTransform(fly_camera.FromWorldToNDC());

        renderer_manager.Render();

        const Rndr::Timestamp end_time = Rndr::GetTimestamp();
        delta_seconds = static_cast<float>(Rndr::GetDuration(start_time, end_time));
    }
}