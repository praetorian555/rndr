#include "opal/time.h"

#include "rndr/application.hpp"
#include "rndr/log.h"
#include "rndr/render-api.h"

int main()
{
    Rndr::Application* app = Rndr::Application::Create();
    if (app == nullptr)
    {
        RNDR_LOG_ERROR("Failed to create app!");
        return -1;
    }
    Rndr::GenericWindow* window = app->CreateGenericWindow();
    if (window == nullptr)
    {
        RNDR_LOG_ERROR("Failed to create window!");
        Rndr::Application::Destroy();
        return -1;
    }
    Rndr::i32 width = 0;
    Rndr::i32 height = 0;
    Rndr::i32 x = 0;
    Rndr::i32 y = 0;
    window->GetPositionAndSize(x, y, width, height);
    window->SetTitle("Window Sample");

    const Rndr::GraphicsContextDesc gc_desc{.window_handle = window->GetNativeHandle()};
    Rndr::GraphicsContext gc{gc_desc};
    const Rndr::SwapChainDesc swap_chain_desc{.width = width, .height = height};
    Rndr::SwapChain swap_chain{gc, swap_chain_desc};

    app->on_window_resize.Bind([&swap_chain](Rndr::GenericWindow*, Rndr::i32 width, Rndr::i32 height)
    {
        swap_chain.SetSize(width, height);
    });

    Rndr::f32 delta_seconds = 0.016f;
    while (!window->IsClosed())
    {
        const Rndr::f64 start_seconds = Opal::GetSeconds();

        app->ProcessSystemEvents();

        gc.ClearAll(Rndr::Vector4f(0.0f, 0.0f, 0.0f, 1.0f));
        gc.Present(swap_chain);

        const Rndr::f64 end_seconds = Opal::GetSeconds();
        delta_seconds = static_cast<Rndr::f32>(end_seconds - start_seconds);
    }

    Rndr::Application::Destroy();
    return 0;
}