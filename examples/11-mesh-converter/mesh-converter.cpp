#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#undef near
#undef far

#include <imgui.h>

#include <filesystem>

#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/renderer-base.h"
#include "rndr/core/window.h"
#include "rndr/render-api/render-api.h"
#include "rndr/utility/imgui-wrapper.h"

class UIRenderer : public Rndr::RendererBase
{
public:
    UIRenderer(const Rndr::String& name, Rndr::Window& window, const Rndr::RendererBaseDesc& desc);
    ~UIRenderer();

    bool Render() override;

    [[nodiscard]] Rndr::String GetOutputFilePath() const { return m_output_file_path; }

private:
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
    window_desc.name = "Mesh Converter";
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

    ImGui::Begin("Mesh Converter Tool");
    if (ImGui::Button("Select file to convert..."))
    {
        m_selected_file_path = OpenFileDialog();
        const std::filesystem::path path(m_selected_file_path);
        m_output_file_path = path.parent_path().string() + "\\" + path.stem().string() + ".rndr";
    }
    ImGui::Text("Selected file: %s", !m_selected_file_path.empty() ? m_selected_file_path.c_str() : "None");
    ImGui::Text("Output file: %s", !m_output_file_path.empty() ? m_output_file_path.c_str() : "None");
    ImGui::Button("Convert");
    ImGui::End();

    Rndr::ImGuiWrapper::EndFrame();
    return true;
}

Rndr::String OpenFileDialog()
{
    OPENFILENAME ofn;
    char fileName[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;  // If you have a window to center over, put its HANDLE here
    ofn.lpstrFilter = "Any File\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select a File";
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

    GetOpenFileName(&ofn);
    return fileName;
}
