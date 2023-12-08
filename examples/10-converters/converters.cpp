#include <filesystem>

#include <imgui.h>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <gli/gli.hpp>
#include <gli/load_ktx.hpp>
#include <gli/texture2d.hpp>

#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/file.h"
#include "rndr/core/renderer-base.h"
#include "rndr/core/window.h"
#include "rndr/render-api/render-api.h"
#include "rndr/utility/cube-map.h"
#include "rndr/utility/imgui-wrapper.h"
#include "rndr/utility/mesh.h"

class UIRenderer : public Rndr::RendererBase
{
public:
    UIRenderer(const Rndr::String& name, Rndr::Window& window, const Rndr::RendererBaseDesc& desc);
    ~UIRenderer();

    bool Render() override;

    [[nodiscard]] Rndr::String GetOutputFilePath() const { return m_output_file_path; }

private:
    void RenderMeshConverterTool();
    void RenderComputeBrdfLutTool();
    void RenderComputeEnvironmentMapTool();

    void ProcessMesh(Rndr::MeshAttributesToLoad attributes_to_load);
    void ComputeBrdfLut(const Rndr::String& output_path, Rndr::String& status);
    void ComputeEnvironmentMap(const Rndr::String& input_path, const Rndr::String& output_path, Rndr::String& status);

    Rndr::String m_selected_file_path;
    Rndr::String m_output_file_path;

    Rndr::Buffer m_brdf_lut_buffer;
    Rndr::Shader m_brdf_lut_shader;
    Rndr::Pipeline m_brdf_lut_pipeline;
    int32_t m_brdf_lut_width = 256;
    int32_t m_brdf_lut_height = 256;
};

void Run();
Rndr::String OpenFileDialog();
Rndr::String OpenFolderDialog();

int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
    return 0;
}

void Run()
{
    Rndr::WindowDesc window_desc;
    window_desc.name = "Converters";
    window_desc.width = 1280;
    window_desc.height = 720;

    Rndr::Window window(window_desc);
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    Rndr::SwapChain swap_chain(graphics_context, {});

    Rndr::RendererBaseDesc renderer_desc;
    renderer_desc.graphics_context = graphics_context;
    renderer_desc.swap_chain = swap_chain;

    Rndr::RendererManager renderer_manager;
    const Rndr::ScopePtr<Rndr::RendererBase> clear_renderer =
        RNDR_MAKE_SCOPED(Rndr::ClearRenderer, "Clear", renderer_desc, Rndr::Vector4f(0.0f, 0.0f, 0.0f, 1.0f));
    const Rndr::ScopePtr<Rndr::RendererBase> ui_renderer = RNDR_MAKE_SCOPED(UIRenderer, "UI", window, renderer_desc);
    const Rndr::ScopePtr<Rndr::RendererBase> present_renderer = RNDR_MAKE_SCOPED(Rndr::PresentRenderer, "Present", renderer_desc);
    renderer_manager.AddRenderer(clear_renderer.get());
    renderer_manager.AddRenderer(ui_renderer.get());
    renderer_manager.AddRenderer(present_renderer.get());

    while (!window.IsClosed())
    {
        window.ProcessEvents();
        renderer_manager.Render();
    }
}

UIRenderer::UIRenderer(const Rndr::String& name, Rndr::Window& window, const Rndr::RendererBaseDesc& desc) : RendererBase(name, desc)
{
    Rndr::ImGuiWrapper::Init(window, *desc.graphics_context, {.display_demo_window = true});

    const uint32_t buffer_size = m_brdf_lut_width * m_brdf_lut_height * sizeof(float) * 2;
    m_brdf_lut_buffer =
        Rndr::Buffer(desc.graphics_context,
                     Rndr::BufferDesc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = buffer_size});
    RNDR_ASSERT(m_brdf_lut_buffer.IsValid());
    const Rndr::String shader_source = Rndr::File::ReadShader(ASSETS_DIR, "compute-brdf.glsl");
    m_brdf_lut_shader = Rndr::Shader(desc.graphics_context, Rndr::ShaderDesc{.type = Rndr::ShaderType::Compute, .source = shader_source});
    RNDR_ASSERT(m_brdf_lut_shader.IsValid());
    m_brdf_lut_pipeline = Rndr::Pipeline(desc.graphics_context, Rndr::PipelineDesc{.compute_shader = &m_brdf_lut_shader});
    RNDR_ASSERT(m_brdf_lut_pipeline.IsValid());
}

UIRenderer::~UIRenderer()
{
    Rndr::ImGuiWrapper::Destroy();
}

bool UIRenderer::Render()
{
    Rndr::ImGuiWrapper::StartFrame();
    RenderMeshConverterTool();
    RenderComputeBrdfLutTool();
    RenderComputeEnvironmentMapTool();
    Rndr::ImGuiWrapper::EndFrame();
    return true;
}

void UIRenderer::RenderMeshConverterTool()
{
    ImGui::Begin("Mesh Converter Tool");
    if (ImGui::Button("Select file to convert..."))
    {
        m_selected_file_path = OpenFileDialog();
        const std::filesystem::path path(m_selected_file_path);
        m_output_file_path = path.parent_path().string() + "\\" + path.stem().string() + ".rndr";
    }
    ImGui::Text("Selected file: %s", !m_selected_file_path.empty() ? m_selected_file_path.c_str() : "None");
    ImGui::Text("Output file: %s", !m_output_file_path.empty() ? m_output_file_path.c_str() : "None");

    Rndr::MeshAttributesToLoad attributes_to_load = Rndr::MeshAttributesToLoad::LoadPositions;
    static bool s_should_load_normals = false;
    static bool s_should_load_uvs = false;
    ImGui::Checkbox("Load Normals", &s_should_load_normals);
    ImGui::Checkbox("Load Uvs", &s_should_load_uvs);
    if (s_should_load_normals)
    {
        attributes_to_load |= Rndr::MeshAttributesToLoad::LoadNormals;
    }
    if (s_should_load_uvs)
    {
        attributes_to_load |= Rndr::MeshAttributesToLoad::LoadUvs;
    }
    if (ImGui::Button("Convert"))
    {
        ProcessMesh(attributes_to_load);
    }
    ImGui::End();
}

void UIRenderer::RenderComputeBrdfLutTool()
{
    ImGui::Begin("Compute BRDF LUT Tool");
    static Rndr::String s_selected_file_path;
    if (ImGui::Button("Select output path..."))
    {
        s_selected_file_path = OpenFolderDialog();
    }
    Rndr::String output_file_path = !s_selected_file_path.empty() ? (s_selected_file_path + "\\brdflut.ktx") : "None";
    ImGui::Text("Output file: %s", output_file_path.c_str());
    static Rndr::String s_status = "Idle";
    if (ImGui::Button("Compute BRDF"))
    {
        if (s_selected_file_path.empty())
        {
            s_status = "No output path selected!";
        }
        else
        {
            ComputeBrdfLut(output_file_path, s_status);
        }
    }
    ImGui::Text("Status: %s", s_status.c_str());
    ImGui::End();
}

void UIRenderer::RenderComputeEnvironmentMapTool()
{
    ImGui::Begin("Compute Environment Map Tool");
    static Rndr::String s_selected_file = "None";
    static Rndr::String s_output_file = "None";
    if (ImGui::Button("Select input environment map..."))
    {
        s_selected_file = OpenFileDialog();
        std::filesystem::path path(s_selected_file);
        Rndr::String selected_directory = path.parent_path().string();
        Rndr::String selected_file_name = path.stem().string();
        Rndr::String selected_file_extension = path.extension().string();
        s_output_file = selected_directory + "\\" + selected_file_name + "_irradience" + selected_file_extension;
    }

    ImGui::Text("Input file: %s", s_selected_file.c_str());
    ImGui::Text("Output file: %s", s_output_file.c_str());

    static Rndr::String s_status = "Idle";
    if (ImGui::Button("Convolve"))
    {
        if (s_output_file.empty())
        {
            s_status = "No output path selected!";
        }
        else
        {
            ComputeEnvironmentMap(s_selected_file, s_output_file, s_status);
        }
    }
    ImGui::Text("Status: %s", s_status.c_str());
    ImGui::End();
}

void UIRenderer::ProcessMesh(Rndr::MeshAttributesToLoad attributes_to_load)
{
    constexpr uint32_t k_ai_process_flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                            aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                                            aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                            aiProcess_GenUVCoords;

    const aiScene* scene = aiImportFile(m_selected_file_path.c_str(), k_ai_process_flags);
    if (scene == nullptr || !scene->HasMeshes())
    {
        RNDR_LOG_ERROR("Failed to load mesh from file with error: %s", aiGetErrorString());
        RNDR_HALT("Invalid mesh file!");
        return;
    }

    Rndr::MeshData mesh_data;
    const bool is_data_loaded = ReadMeshData(mesh_data, *scene, attributes_to_load);
    if (!is_data_loaded)
    {
        RNDR_LOG_ERROR("Failed to load mesh data from file: %s", m_selected_file_path.c_str());
        RNDR_HALT("Failed  to load mesh data!");
        return;
    }
    aiReleaseImport(scene);

    const bool is_data_written = WriteOptimizedMeshData(mesh_data, m_output_file_path);
    if (!is_data_written)
    {
        RNDR_LOG_ERROR("Failed to write mesh data to file: %s", m_output_file_path.c_str());
        RNDR_HALT("Failed to write mesh data!");
        return;
    }
}

void UIRenderer::ComputeBrdfLut(const Rndr::String& output_path, Rndr::String& status)
{
    RNDR_UNUSED(output_path);
    m_desc.graphics_context->Bind(m_brdf_lut_buffer, 0);
    m_desc.graphics_context->Bind(m_brdf_lut_pipeline);
    if (!m_desc.graphics_context->DispatchCompute(m_brdf_lut_width, m_brdf_lut_height, 1))
    {
        status = "Failed to dispatch compute shader!";
        return;
    }

    Rndr::Array<float> read_data_storage(m_brdf_lut_width * m_brdf_lut_height * 2);
    Rndr::ByteSpan read_data = Rndr::ToByteSpan(read_data_storage);
    if (!m_desc.graphics_context->Read(m_brdf_lut_buffer, read_data))
    {
        status = "Failed to read buffer data!";
        return;
    }

    gli::texture lut_texture = gli::texture2d(gli::FORMAT_RG16_SFLOAT_PACK16, gli::extent2d(m_brdf_lut_width, m_brdf_lut_height), 1);

    for (int y = 0; y < m_brdf_lut_height; y++)
    {
        for (int x = 0; x < m_brdf_lut_width; x++)
        {
            const int ofs = y * m_brdf_lut_width + x;
            const gli::vec2 value(read_data_storage[ofs * 2 + 0], read_data_storage[ofs * 2 + 1]);
            const gli::texture::extent_type uv = {x, y, 0};
            lut_texture.store<glm::uint32>(uv, 0, 0, 0, gli::packHalf2x16(value));
        }
    }

    if (!gli::save_ktx(lut_texture, output_path))
    {
        status = "Failed to save BRDF LUT to file!";
        return;
    }

    status = "BRDF LUT computed successfully!";
}

void UIRenderer::ComputeEnvironmentMap(const Rndr::String& input_path, const Rndr::String& output_path, Rndr::String& status)
{
    Rndr::Bitmap input_bitmap = Rndr::File::ReadEntireImage(input_path, Rndr::PixelFormat::R32G32B32_FLOAT);
    if (!input_bitmap.IsValid())
    {
        status = "Failed to read input image!";
        return;
    }

    const int32_t input_width = input_bitmap.GetWidth();
    const int32_t input_height = input_bitmap.GetHeight();
    Rndr::Vector3f* input_data = reinterpret_cast<Rndr::Vector3f*>(input_bitmap.GetData());

    const int32_t output_width = 256;
    const int32_t output_height = 128;
    Rndr::Array<Rndr::Vector3f> output_data(output_width * output_height);

    constexpr int32_t k_nb_monte_carlo_samples = 1024;
    if (!Rndr::CubeMap::ConvolveDiffuse(input_data, input_width, input_height, output_width, output_height, output_data.data(),
                                        k_nb_monte_carlo_samples))
    {
        status = "Failed to convolve input image!";
        return;
    }

    Rndr::Bitmap output_bitmap(output_width, output_height, 1, Rndr::PixelFormat::R32G32B32_FLOAT, Rndr::ToByteSpan(output_data));
    if (!Rndr::File::SaveImage(output_bitmap, output_path))
    {
        status = "Failed to save output image!";
        return;
    }

    status = "Environment map convolved successfully!";
}
