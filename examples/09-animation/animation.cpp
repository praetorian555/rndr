#include "rndr/rndr.h"

#include <unordered_map>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>
#include <assimp/matrix4x4.h>

#include "animator.h"

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
    Rndr::Init({.enable_input_system = true, .enable_cpu_tracer = true});
    Run();
    Rndr::Destroy();
}

const char* const g_shader_code_vertex = R"(
#version 460 core
const int k_max_bones = 100;
const int k_max_influences = 4;
layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 mvp;
    uniform mat4 bone_transforms[k_max_bones];
};
layout (location=0) in vec3 pos;
layout (location=1) in ivec4 bone_ids;
layout (location=2) in vec4 bone_weights;
layout (location=3) in vec2 tex_coords;
layout (location=0) out vec2 out_tex_coords;
void main()
{
    vec4 position = vec4(0.0);
    for (int i = 0; i < k_max_influences; i++)
    {
        if (bone_ids[i] == -1)
        {
            continue;
        }
        if(bone_ids[i] >= k_max_bones)
        {
            position = vec4(pos, 1.0f);
            break;
        }
        const mat4 bone_transform = bone_transforms[bone_ids[i]];
        position += (bone_transform * vec4(pos, 1.0)) * bone_weights[i];
    }
	gl_Position = mvp * position;
    //gl_Position = mvp * vec4(pos, 1.0);
	out_tex_coords = tex_coords;
}
)";

const char* const g_shader_code_fragment = R"(
#version 460 core
layout (location=0) in vec2 tex_coords;
layout (location=0) out vec4 out_frag_color;
uniform sampler2D diffuse_texture;
void main()
{
	out_frag_color = texture(diffuse_texture, tex_coords);
    out_frag_color.a = 1.0;
};
)";

struct VertexData
{
    constexpr static size_t k_max_bone_influence_count = 4;

    Rndr::Point3f position;
    Rndr::StackArray<int32_t, k_max_bone_influence_count> bone_ids;
    Rndr::StackArray<float, k_max_bone_influence_count> bone_weights;
    Rndr::Point2f tex_coords;
};

struct Mesh
{
    std::vector<VertexData> vertex_data;
    std::vector<int32_t> indices;
    BoneInfoMap bone_name_to_bone_info;

    Rndr::String diffuse_texture_path;
};

struct PerFrameData
{
    constexpr static size_t k_max_bone_count = 100;

    Rndr::Matrix4x4f mvp;
    Rndr::StackArray<Rndr::Matrix4x4f, k_max_bone_count> final_bone_transforms;
};

class MeshRenderer : public Rndr::RendererBase
{
public:
    MeshRenderer(const Rndr::String& name, const Rndr::RendererBaseDesc& desc) : Rndr::RendererBase(name, desc)
    {
        const bool is_model_loaded = LoadModel(m_mesh);
        RNDR_ASSERT(is_model_loaded);
        RNDR_UNUSED(is_model_loaded);

        const bool is_animation_loaded = LoadAnimation(m_animation, m_mesh);
        RNDR_ASSERT(is_animation_loaded);
        RNDR_UNUSED(is_animation_loaded);

        StartAnimation(m_animator, m_animation);

        m_index_count = static_cast<int32_t>(m_mesh.indices.size());

        m_vertex_shader = RNDR_MAKE_SCOPED(Rndr::Shader, m_desc.graphics_context,
                                           Rndr::ShaderDesc{.type = Rndr::ShaderType::Vertex, .source = g_shader_code_vertex});
        RNDR_ASSERT(m_vertex_shader->IsValid());
        m_pixel_shader = RNDR_MAKE_SCOPED(Rndr::Shader, m_desc.graphics_context,
                                          Rndr::ShaderDesc{.type = Rndr::ShaderType::Fragment, .source = g_shader_code_fragment});
        RNDR_ASSERT(m_pixel_shader->IsValid());

        constexpr size_t k_stride = sizeof(VertexData);
        m_vertex_buffer = RNDR_MAKE_SCOPED(Rndr::Buffer, m_desc.graphics_context,
                                           {.type = Rndr::BufferType::Vertex,
                                            .usage = Rndr::Usage::Default,
                                            .size = static_cast<uint32_t>(k_stride * m_mesh.vertex_data.size()),
                                            .stride = k_stride},
                                           Rndr::ToByteSpan(m_mesh.vertex_data));
        RNDR_ASSERT(m_vertex_buffer->IsValid());
        m_index_buffer = RNDR_MAKE_SCOPED(Rndr::Buffer, m_desc.graphics_context,
                                          {.type = Rndr::BufferType::Index,
                                           .usage = Rndr::Usage::Default,
                                           .size = static_cast<uint32_t>(sizeof(int32_t) * m_mesh.indices.size()),
                                           .stride = sizeof(int32_t)},
                                          Rndr::ToByteSpan(m_mesh.indices));

        Rndr::InputLayoutBuilder builder;
        const Rndr::InputLayoutDesc input_layout_desc = builder.AddVertexBuffer(*m_vertex_buffer, 0, Rndr::DataRepetition::PerVertex)
                                                            .AppendElement(0, Rndr::PixelFormat::R32G32B32_FLOAT)
                                                            .AppendElement(0, Rndr::PixelFormat::R32G32B32A32_SINT)
                                                            .AppendElement(0, Rndr::PixelFormat::R32G32B32A32_FLOAT)
                                                            .AppendElement(0, Rndr::PixelFormat::R32G32_FLOAT)
                                                            .AddIndexBuffer(*m_index_buffer)
                                                            .Build();

        m_pipeline = RNDR_MAKE_SCOPED(Rndr::Pipeline, m_desc.graphics_context,
                                      Rndr::PipelineDesc{.vertex_shader = m_vertex_shader.get(),
                                                         .pixel_shader = m_pixel_shader.get(),
                                                         .input_layout = input_layout_desc,
                                                         .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                         .depth_stencil = {.is_depth_enabled = true}});
        RNDR_ASSERT(m_pipeline->IsValid());

        if (!m_mesh.diffuse_texture_path.empty())
        {
            const Rndr::String file_path = Rndr::String(ASSETS_DIR) + "/" + m_mesh.diffuse_texture_path;
            Rndr::Bitmap diffuse_bitmap = Rndr::File::ReadEntireImage(file_path, Rndr::PixelFormat::R8G8B8A8_UNORM_SRGB, true);
            RNDR_ASSERT(diffuse_bitmap.IsValid());
            m_diffuse_image = RNDR_MAKE_SCOPED(Rndr::Image, m_desc.graphics_context, diffuse_bitmap, false, {});
            RNDR_ASSERT(m_diffuse_image->IsValid());
        }

        constexpr size_t k_per_frame_size = sizeof(PerFrameData);
        m_per_frame_buffer = RNDR_MAKE_SCOPED(
            Rndr::Buffer, m_desc.graphics_context,
            {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
        RNDR_ASSERT(m_per_frame_buffer->IsValid());
    }

    static bool LoadModel(Mesh& out_mesh)
    {
        const Rndr::String file_path = ASSETS_DIR "vampire-dancing.dae";
        const aiScene* scene = aiImportFile(file_path.c_str(), aiProcess_Triangulate);
        if (scene == nullptr || !scene->HasMeshes())
        {
            RNDR_LOG_ERROR("Failed to load mesh from file with error: %s", aiGetErrorString());
            return false;
        }
        const aiMesh* mesh = scene->mMeshes[0];
        for (uint32_t i = 0; i < mesh->mNumVertices; i++)
        {
            const aiVector3D v = mesh->mVertices[i];
            Rndr::Point2f tex_coords{0.0f, 0.0f};
            if (mesh->HasTextureCoords(0))
            {
                tex_coords.x = mesh->mTextureCoords[0][i].x;
                tex_coords.y = mesh->mTextureCoords[0][i].y;
            }
            const VertexData vertex_data{Rndr::Point3f(v.x, v.y, v.z), {-1, -1, -1, -1}, {0.0f, 0.0f, 0.0f, 0.0f}, tex_coords};
            out_mesh.vertex_data.emplace_back(vertex_data);
        }
        for (uint32_t i = 0; i != mesh->mNumFaces; i++)
        {
            const aiFace& face = mesh->mFaces[i];
            for (uint32_t j = 0; j != 3; j++)
            {
                out_mesh.indices.emplace_back(face.mIndices[j]);
            }
        }
        const int32_t bone_count = static_cast<int32_t>(mesh->mNumBones);
        for (int32_t bone_index = 0; bone_index < bone_count; bone_index++)
        {
            const aiBone* bone = mesh->mBones[bone_index];
            const Rndr::String bone_name = bone->mName.C_Str();
            RNDR_ASSERT(out_mesh.bone_name_to_bone_info.find(bone_name) == out_mesh.bone_name_to_bone_info.end());
            const BoneInfo bone_info{bone_index, AssimpMatrixToMatrix4x4(bone->mOffsetMatrix)};
            out_mesh.bone_name_to_bone_info[bone_name] = bone_info;
            for (uint32_t weight_index = 0; weight_index < bone->mNumWeights; weight_index++)
            {
                const aiVertexWeight& weight = bone->mWeights[weight_index];
                VertexData& vertex_data = out_mesh.vertex_data[weight.mVertexId];
                for (uint32_t influence_index = 0; influence_index < VertexData::k_max_bone_influence_count; influence_index++)
                {
                    if (vertex_data.bone_ids[influence_index] == -1)
                    {
                        vertex_data.bone_ids[influence_index] = bone_index;
                        vertex_data.bone_weights[influence_index] = weight.mWeight;
                        break;
                    }
                }
            }
        }
        RNDR_ASSERT(scene->HasMaterials());
        const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        aiString texture_path;
        material->GetTexture(aiTextureType_DIFFUSE, 0, &texture_path);
        out_mesh.diffuse_texture_path = texture_path.C_Str();
        aiReleaseImport(scene);
        return true;
    }

    static bool LoadAnimation(Animation& out_animation, Mesh& mesh)
    {
        const Rndr::String file_path = ASSETS_DIR "vampire-dancing.dae";
        const aiScene* scene = aiImportFile(file_path.c_str(), aiProcess_Triangulate);
        if (scene == nullptr || !scene->HasAnimations())
        {
            RNDR_LOG_ERROR("Failed to load animation from file with error: %s", aiGetErrorString());
            return false;
        }
        return LoadAnimationFromAssimp(out_animation, scene, 0, mesh.bone_name_to_bone_info);
    }
    
    bool Render() override
    {
        RNDR_TRACE_SCOPED(Mesh rendering);

        const Rndr::Matrix4x4f t = Math::Translate(Rndr::Vector3f(0.0f, -1.0f, -5.f));
        Rndr::Matrix4x4f mvp = m_camera_transform * t;
        mvp = Math::Transpose(mvp);
        PerFrameData per_frame_data = {.mvp = mvp};
        Rndr::Span<Rndr::Matrix4x4f> final_bone_transforms{per_frame_data.final_bone_transforms};
        UpdateAnimator(final_bone_transforms, m_animator, m_delta_seconds);
        m_desc.graphics_context->Update(*m_per_frame_buffer, Rndr::ToByteSpan(per_frame_data));

        // Bind resources
        m_desc.graphics_context->Bind(*m_desc.swap_chain);
        m_desc.graphics_context->Bind(*m_pipeline);
        m_desc.graphics_context->Bind(*m_per_frame_buffer, 0);
        if (m_diffuse_image->IsValid())
        {
            m_desc.graphics_context->Bind(*m_diffuse_image, 0);
        }

        // Draw
        m_desc.graphics_context->DrawIndices(Rndr::PrimitiveTopology::Triangle, m_index_count);

        return true;
    }

    void SetCameraTransform(const Rndr::Matrix4x4f& transform) { m_camera_transform = transform; }

    void SetDeltaSeconds(float delta_seconds) { m_delta_seconds = delta_seconds; }

private:
    Rndr::ScopePtr<Rndr::Shader> m_vertex_shader;
    Rndr::ScopePtr<Rndr::Shader> m_pixel_shader;
    Rndr::ScopePtr<Rndr::Buffer> m_vertex_buffer;
    Rndr::ScopePtr<Rndr::Buffer> m_index_buffer;
    Rndr::ScopePtr<Rndr::Pipeline> m_pipeline;
    Rndr::ScopePtr<Rndr::Buffer> m_per_frame_buffer;
    Rndr::ScopePtr<Rndr::Image> m_diffuse_image;

    int32_t m_index_count = 0;
    Rndr::Matrix4x4f m_camera_transform;
    Mesh m_mesh;
    Animation m_animation;
    Animator m_animator;
    float m_delta_seconds;
};

void Run()
{
    Rndr::Window window({.width = 800, .height = 600, .name = "Animation Example"});
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
    const Rndr::ScopePtr<MeshRenderer> mesh_renderer = RNDR_MAKE_SCOPED(MeshRenderer, "Render a mesh", renderer_desc);

    const Rndr::ProjectionCamera camera(Rndr::Point3f(0.0f, 0.0f, 0.0f), Rndr::Rotatorf(0.0f, 0.0f, 0.0f), window.GetWidth(),
                                        window.GetHeight(), {.near = 0.1f, .far = 1000.0f});
    mesh_renderer->SetCameraTransform(camera.FromWorldToNDC());

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

        mesh_renderer->SetDeltaSeconds(delta_seconds);

        fps_counter.Update(delta_seconds);

        window.ProcessEvents();
        Rndr::InputSystem::ProcessEvents(delta_seconds);

        renderer_manager.Render();

        const Rndr::Timestamp end_time = Rndr::GetTimestamp();
        delta_seconds = static_cast<float>(Rndr::GetDuration(start_time, end_time));
    }
}