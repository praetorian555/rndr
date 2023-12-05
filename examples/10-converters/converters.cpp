#include <filesystem>

#include <imgui.h>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/renderer-base.h"
#include "rndr/core/window.h"
#include "rndr/render-api/render-api.h"
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

    void ProcessMesh(Rndr::MeshAttributesToLoad attributes_to_load);

    Rndr::String m_selected_file_path;
    Rndr::String m_output_file_path;
};

void Run();
Rndr::String OpenFileDialog();

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
}

UIRenderer::~UIRenderer()
{
    Rndr::ImGuiWrapper::Destroy();
}

bool UIRenderer::Render()
{
    Rndr::ImGuiWrapper::StartFrame();
    RenderMeshConverterTool();
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
