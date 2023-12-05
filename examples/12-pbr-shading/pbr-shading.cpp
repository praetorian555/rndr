#include "rndr/core/base.h"
#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/file.h"
#include "rndr/core/input.h"
#include "rndr/core/renderer-base.h"  // TODO: Rename renderer-base.h to renderer-manager.h
#include "rndr/core/time.h"
#include "rndr/core/window.h"
#include "rndr/render-api/render-api.h"
#include "rndr/utility/fly-camera.h"
#include "rndr/utility/input-layout-builder.h"
#include "rndr/utility/mesh.h"

void Run(const Rndr::String& asset_path);

int main(int argc, char* argv[])
{
    Rndr::Init({.enable_input_system = true});
    if (argc > 1)
    {
        Run(argv[1]);
    }
    else
    {
        RNDR_LOG_ERROR("No asset path provided!");
    }
    Rndr::Destroy();
    return 0;
}

struct InstanceData
{
    Rndr::Matrix4x4f model;
    Rndr::Matrix4x4f normal;
};

struct PerFrameData
{
    Rndr::Matrix4x4f view_projection;
    Rndr::Point3f camera_position;
};

class PbrRenderer : public Rndr::RendererBase
{
public:
    PbrRenderer(const Rndr::String& name, const Rndr::RendererBaseDesc& desc, const Rndr::String& asset_path)
        : Rndr::RendererBase(name, desc), m_asset_path(asset_path)
    {
        using namespace Rndr;
        const String vertex_shader_code = Rndr::File::ReadShader(ASSETS_DIR, "basic-pbr.vert");
        const String fragment_shader_code = Rndr::File::ReadShader(ASSETS_DIR, "basic-pbr.frag");
        m_vertex_shader = Shader(desc.graphics_context, {.type = ShaderType::Vertex, .source = vertex_shader_code});
        RNDR_ASSERT(m_vertex_shader.IsValid());
        m_fragment_shader = Shader(desc.graphics_context, {.type = ShaderType::Fragment, .source = fragment_shader_code});
        RNDR_ASSERT(m_fragment_shader.IsValid());

        const Rndr::String mesh_path = m_asset_path + "/DamagedHelmet.rndr";
        if (!Rndr::ReadOptimizedMeshData(m_mesh_data, mesh_path))
        {
            RNDR_LOG_ERROR("Failed to load mesh data from file: %s", mesh_path.c_str());
            exit(1);
        }

        m_vertex_buffer = Rndr::Buffer(desc.graphics_context,
                                       {.type = Rndr::BufferType::Vertex,
                                        .usage = Rndr::Usage::Default,
                                        .size = static_cast<uint32_t>(m_mesh_data.vertex_buffer_data.size())},
                                       m_mesh_data.vertex_buffer_data);
        RNDR_ASSERT(m_vertex_buffer.IsValid());
        m_index_buffer = Rndr::Buffer(desc.graphics_context,
                                      {.type = Rndr::BufferType::Index,
                                       .usage = Rndr::Usage::Default,
                                       .size = static_cast<uint32_t>(m_mesh_data.index_buffer_data.size()),
                                       .stride = sizeof(uint32_t)},
                                      m_mesh_data.index_buffer_data);
        RNDR_ASSERT(m_index_buffer.IsValid());

        m_instance_buffer = Rndr::Buffer(desc.graphics_context,
                                         {.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = sizeof(InstanceData)});
        RNDR_ASSERT(m_instance_buffer.IsValid());

        m_per_frame_buffer = Rndr::Buffer(
            desc.graphics_context, {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = sizeof(PerFrameData)});
        RNDR_ASSERT(m_per_frame_buffer.IsValid());

        const Rndr::InputLayoutDesc input_layout_desc = Rndr::InputLayoutBuilder()
                                                            .AddVertexBuffer(m_vertex_buffer, 1, Rndr::DataRepetition::PerVertex)
                                                            .AddVertexBuffer(m_instance_buffer, 2, Rndr::DataRepetition::PerInstance, 1)
                                                            .AddIndexBuffer(m_index_buffer)
                                                            .Build();

        m_pipeline = Rndr::Pipeline(desc.graphics_context, {.vertex_shader = &m_vertex_shader,
                                                            .pixel_shader = &m_fragment_shader,
                                                            .input_layout = input_layout_desc,
                                                            .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                            .depth_stencil = {.is_depth_enabled = true}});
        RNDR_ASSERT(m_pipeline.IsValid());

        const Rndr::Matrix4x4f model_transform(1.0f);
        InstanceData instance_data = {.model = model_transform, .normal = model_transform};
        m_desc.graphics_context->Update(m_instance_buffer, Rndr::ToByteSpan(instance_data));

        m_command_list = Rndr::CommandList(desc.graphics_context);
        m_command_list.BindConstantBuffer(m_per_frame_buffer, 0);
        m_command_list.Bind(m_pipeline);
        m_command_list.DrawIndices(Rndr::PrimitiveTopology::Triangle, static_cast<int32_t>(m_mesh_data.index_buffer_data.size()));
    }

    bool Render() override
    {
        const Rndr::Matrix4x4f view_projection_transform = Math::Transpose(m_camera_transform);
        PerFrameData per_frame_data = {.view_projection = view_projection_transform, .camera_position = m_camera_position};
        m_desc.graphics_context->Update(m_per_frame_buffer, Rndr::ToByteSpan(per_frame_data));

        m_command_list.Submit();
        return true;
    }

    void SetCameraInfo(const Rndr::Matrix4x4f& transform, const Rndr::Point3f& position)
    {
        m_camera_transform = transform;
        m_camera_position = position;
    }

private:
    Rndr::String m_asset_path;

    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_fragment_shader;
    Rndr::MeshData m_mesh_data;
    Rndr::Buffer m_vertex_buffer;
    Rndr::Buffer m_index_buffer;
    Rndr::Buffer m_instance_buffer;
    Rndr::Buffer m_per_frame_buffer;
    Rndr::Pipeline m_pipeline;
    Rndr::CommandList m_command_list;

    Rndr::Matrix4x4f m_camera_transform;
    Rndr::Point3f m_camera_position;
};

void Run(const Rndr::String& asset_path)
{
    using namespace Rndr;
    Window window({.width = 1280, .height = 720, .name = "PBR Shading"});
    GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});

    FlyCamera fly_camera(&window, &InputSystem::GetCurrentContext(),
                         {.start_position = Point3f(0.0f, 500.0f, 0.0f),
                          .movement_speed = 100,
                          .rotation_speed = 200,
                          .projection_desc = {.near = 0.5f, .far = 5000.0f}});

    SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight()});
    const RendererBaseDesc renderer_desc{.graphics_context = Ref{graphics_context}, .swap_chain = Ref{swap_chain}};
    RendererManager renderer_manager;
    const ScopePtr<RendererBase> clear_renderer = MakeScoped<ClearRenderer>(String("Clear"), renderer_desc, Colors::k_white);
    const ScopePtr<PbrRenderer> pbr_renderer = MakeScoped<PbrRenderer>(String("PBR"), renderer_desc, asset_path);
    const ScopePtr<RendererBase> present_renderer = MakeScoped<PresentRenderer>(String("Present"), renderer_desc);
    renderer_manager.AddRenderer(clear_renderer.get());
    renderer_manager.AddRenderer(pbr_renderer.get());
    renderer_manager.AddRenderer(present_renderer.get());

    float delta_seconds = 1 / 60.0f;
    while (!window.IsClosed())
    {
        const Timestamp start_time = GetTimestamp();

        window.ProcessEvents();
        InputSystem::ProcessEvents(delta_seconds);

        fly_camera.Update(delta_seconds);
        pbr_renderer->SetCameraInfo(fly_camera.FromWorldToNDC(), fly_camera.GetPosition());

        renderer_manager.Render();

        const Timestamp end_time = GetTimestamp();
        delta_seconds = static_cast<float>(GetDuration(start_time, end_time));
    }
}