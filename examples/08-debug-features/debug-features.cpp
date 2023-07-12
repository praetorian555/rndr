#include "rndr/rndr.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>

void Run();

/**
 * In this example you will learn how to:
 *     1. Load a mesh from a file using Assimp.
 *     2. Render a mesh using just vertices, with no index buffers.
 *     3. Update uniform buffers per frame.
 *     4. Render wireframes.
 *     5. Use math transformations.
 */
int main()
{
    Rndr::Init({.enable_cpu_tracer = true});
    Run();
    Rndr::Destroy();
}

const char* const g_shader_code_vertex = R"(
#version 460 core
layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 MVP;
	uniform int isWireframe;
};
layout (location=0) in vec3 pos;
layout (location=0) out vec3 color;
void main()
{
	gl_Position = MVP * vec4(pos, 1.0);
	color = isWireframe > 0 ? vec3(0.0f) : pos.xyz;
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
    math::Matrix4x4 mvp;
    int is_wire_frame;
};

class MeshRenderer : public Rndr::RendererBase
{
public:
    MeshRenderer(const Rndr::String& name, const Rndr::RendererBaseDesc& desc) : Rndr::RendererBase(name, desc)
    {
        const Rndr::String file_path = ASSETS_DIR "duck.gltf";
        const aiScene* scene = aiImportFile(file_path.c_str(), aiProcess_Triangulate);
        if (scene == nullptr || !scene->HasMeshes())
        {
            RNDR_LOG_ERROR("Failed to load mesh from file with error: %s", aiGetErrorString());
            assert(false);
            return;
        }
        assert(scene->HasMeshes());
        const aiMesh* mesh = scene->mMeshes[0];
        Rndr::Array<math::Point3> positions;
        for (unsigned int i = 0; i != mesh->mNumFaces; i++)
        {
            const aiFace& face = mesh->mFaces[i];
            Rndr::StackArray<size_t, 3> idx{face.mIndices[0], face.mIndices[1], face.mIndices[2]};
            for (int j = 0; j != 3; j++)
            {
                const aiVector3D v = mesh->mVertices[idx[j]];
                positions.emplace_back(math::Point3(v.x, v.y, v.z));  // NOLINT
            }
        }
        aiReleaseImport(scene);
        m_vertex_count = static_cast<int32_t>(positions.size());

        m_vertex_shader = RNDR_MAKE_SCOPED(Rndr::Shader, m_desc.graphics_context,
                                           Rndr::ShaderDesc{.type = Rndr::ShaderType::Vertex, .source = g_shader_code_vertex});
        assert(m_vertex_shader->IsValid());
        m_pixel_shader = RNDR_MAKE_SCOPED(Rndr::Shader, m_desc.graphics_context,
                                          Rndr::ShaderDesc{.type = Rndr::ShaderType::Fragment, .source = g_shader_code_fragment});
        assert(m_pixel_shader->IsValid());

        constexpr size_t k_stride = sizeof(math::Point3);
        m_vertex_buffer = RNDR_MAKE_SCOPED(Rndr::Buffer, m_desc.graphics_context,
                                           {.type = Rndr::BufferType::Vertex,
                                            .usage = Rndr::Usage::Default,
                                            .size = static_cast<uint32_t>(k_stride * positions.size()),
                                            .stride = k_stride},
                                           Rndr::ToByteSpan(positions));
        assert(m_vertex_buffer->IsValid());
        Rndr::InputLayoutBuilder builder;
        const Rndr::InputLayoutDesc input_layout_desc = builder.AddVertexBuffer(*m_vertex_buffer, 0, Rndr::DataRepetition::PerVertex)
                                                            .AppendElement(0, Rndr::PixelFormat::R32G32B32_FLOAT)
                                                            .Build();

        m_pipeline = RNDR_MAKE_SCOPED(Rndr::Pipeline, m_desc.graphics_context,
                                      Rndr::PipelineDesc{.vertex_shader = m_vertex_shader.get(),
                                                         .pixel_shader = m_pixel_shader.get(),
                                                         .input_layout = input_layout_desc,
                                                         .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                         .depth_stencil = {.is_depth_enabled = true}});
        assert(m_pipeline->IsValid());

        constexpr size_t k_per_frame_size = sizeof(PerFrameData);
        m_per_frame_buffer = RNDR_MAKE_SCOPED(
            Rndr::Buffer, m_desc.graphics_context,
            {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
        assert(m_per_frame_buffer->IsValid());
    }

    bool Render() override
    {
        RNDR_TRACE_SCOPED(Mesh rendering);

        // Rotate the mesh
        const float width = static_cast<float>(m_desc.swap_chain->GetDesc().width);
        const float height = static_cast<float>(m_desc.swap_chain->GetDesc().height);
        const float ratio = width / height;
        const float angle = static_cast<float>(std::fmod(10 * Rndr::GetSystemTime(), 360.0));
        const math::Transform t =
            math::Translate(math::Vector3(0.0f, -0.5f, -1.5f)) * math::Rotate(angle, math::Vector3(0.0f, 1.0f, 0.0f)) * math::RotateX(-90);
        const math::Matrix4x4 p = math::Perspective_RH_N1(45.0f, ratio, 0.1f, 1000.0f);
        math::Matrix4x4 mvp = math::Multiply(p, t.GetMatrix());
        mvp = mvp.Transpose();
        PerFrameData per_frame_data = {.mvp = mvp, .is_wire_frame = 0};
        m_desc.graphics_context->Update(*m_per_frame_buffer, Rndr::ToByteSpan(per_frame_data));

        // Bind resources
        m_desc.graphics_context->Bind(*m_desc.swap_chain);
        m_desc.graphics_context->Bind(*m_pipeline);
        m_desc.graphics_context->BindUniform(*m_per_frame_buffer, 0);

        // Draw
        m_desc.graphics_context->DrawVertices(Rndr::PrimitiveTopology::Triangle, m_vertex_count);

        return true;
    }

private:
    Rndr::ScopePtr<Rndr::Shader> m_vertex_shader;
    Rndr::ScopePtr<Rndr::Shader> m_pixel_shader;
    Rndr::ScopePtr<Rndr::Buffer> m_vertex_buffer;
    Rndr::ScopePtr<Rndr::Pipeline> m_pipeline;
    Rndr::ScopePtr<Rndr::Buffer> m_per_frame_buffer;

    int32_t m_vertex_count = 0;
};

void Run()
{
    Rndr::Window window({.width = 800, .height = 600, .name = "Debug Features Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    assert(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight(), .enable_vsync = false});
    assert(swap_chain.IsValid());

    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height) { swap_chain.SetSize(width, height); });

    const Rndr::RendererBaseDesc renderer_desc = {.graphics_context = Rndr::Ref{graphics_context}, .swap_chain = Rndr::Ref{swap_chain}};

    constexpr math::Vector4 k_clear_color{MATH_REALC(1.0), MATH_REALC(1.0), MATH_REALC(1.0), MATH_REALC(1.0)};
    const Rndr::ScopePtr<Rndr::RendererBase> clear_renderer =
        RNDR_MAKE_SCOPED(Rndr::ClearRenderer, "Clear the screen", renderer_desc, k_clear_color);
    const Rndr::ScopePtr<Rndr::RendererBase> present_renderer =
        RNDR_MAKE_SCOPED(Rndr::PresentRenderer, "Present the back buffer", renderer_desc);
    const Rndr::ScopePtr<Rndr::RendererBase> mesh_renderer = RNDR_MAKE_SCOPED(MeshRenderer, "Render a mesh", renderer_desc);
    const Rndr::ScopePtr<Rndr::LineRenderer> line_renderer = RNDR_MAKE_SCOPED(Rndr::LineRenderer, "Debug renderer", renderer_desc);

    Rndr::RendererManager renderer_manager;
    renderer_manager.AddRenderer(clear_renderer.get());
    renderer_manager.AddRenderer(mesh_renderer.get());
    renderer_manager.AddRenderer(present_renderer.get());
    renderer_manager.AddRendererBefore(line_renderer.get(), "Present the back buffer");

    Rndr::FramesPerSecondCounter fps_counter(0.1f);
    float delta_seconds = 0.033f;
    while (!window.IsClosed())
    {
        RNDR_TRACE_SCOPED(Frame);

        const Rndr::Timestamp start_time = Rndr::GetTimestamp();

        fps_counter.Update(delta_seconds);

        window.ProcessEvents();

        line_renderer->AddLine(math::Point3(-1.0f, -0.5f, -0.5f), math::Point3(1.0f, 0.5f, -0.5f), math::Vector4(1.0f, 0.0f, 0.0f, 1.0f));

        renderer_manager.Render();

        const Rndr::Timestamp end_time = Rndr::GetTimestamp();
        delta_seconds = static_cast<float>(Rndr::GetDuration(start_time, end_time));
    }
}