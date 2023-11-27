#include "rndr/core/base.h"
#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/input.h"
#include "rndr/core/renderer-base.h"  // TODO: Rename renderer-base.h to renderer-manager.h
#include "rndr/core/time.h"
#include "rndr/core/window.h"
#include "rndr/render-api/render-api.h"

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

class PbrRenderer : public Rndr::RendererBase
{
public:
    PbrRenderer(const Rndr::String& name, const Rndr::RendererBaseDesc& desc, const Rndr::String& asset_path)
        : Rndr::RendererBase(name, desc), m_asset_path(asset_path)
    {
    }

    bool Render() override { return true; }

private:
    Rndr::String m_asset_path;
};

void Run(const Rndr::String& asset_path)
{
    using namespace Rndr;
    const Window window({.width = 1280, .height = 720, .name = "PBR Shading"});
    GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight()});
    const RendererBaseDesc renderer_desc{.graphics_context = Ref{graphics_context}, .swap_chain = Ref{swap_chain}};
    RendererManager renderer_manager;
    const ScopePtr<RendererBase> clear_renderer = MakeScoped<ClearRenderer>(String("Clear"), renderer_desc, Colors::k_white);
    const ScopePtr<RendererBase> pbr_renderer = MakeScoped<PbrRenderer>(String("PBR"), renderer_desc, asset_path);
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
        renderer_manager.Render();

        const Timestamp end_time = GetTimestamp();
        delta_seconds = static_cast<float>(GetDuration(start_time, end_time));
    }
}