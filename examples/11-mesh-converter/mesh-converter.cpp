#include "rndr/rndr.h"

class UIRenderer : public Rndr::RendererBase
{
public:
    UIRenderer(const Rndr::String& name, Rndr::Window& window, const Rndr::RendererBaseDesc& desc);
    ~UIRenderer();

    bool Render() override;
};

void Run();

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
    Rndr::ImGuiWrapper::EndFrame();
    return true;
}
