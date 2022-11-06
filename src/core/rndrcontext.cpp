#include "rndr/core/rndrcontext.h"

#include <cassert>
#include <chrono>

#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/rndrcontext.h"

#include "rndr/profiling/cputracer.h"

#include "rndr/render/graphicscontext.h"
#include "rndr/render/image.h"

static rndr::DefaultAllocator g_DefaultAllocator;

rndr::RndrContext::RndrContext(const RndrContextProperties& Props)
{
    if (Props.UserAllocator)
    {
        m_Allocator = Props.UserAllocator;
    }
    else
    {
        m_Allocator = &g_DefaultAllocator;
    }

    StdAsyncLogger::Get()->Init();
}

rndr::RndrContext::~RndrContext()
{
    StdAsyncLogger::Get()->ShutDown();
}

rndr::Window* rndr::RndrContext::CreateWin(int Width, int Height, const WindowProperties& Props)
{
    Window* W = RNDR_NEW(this, Window, "rndr::RndrContext: Window", Width, Height, Props);
    return W;
}

rndr::GraphicsContext* rndr::RndrContext::CreateGraphicsContext(const GraphicsContextProperties& Props)
{
    GraphicsContext* GC = RNDR_NEW(this, GraphicsContext, "rndr::RndrContext: GraphicsContext");
    if (!GC || !GC->Init(this, Props))
    {
        RNDR_DELETE(this, GraphicsContext, GC);
    }
    return GC;
}

rndr::InputSystem* rndr::RndrContext::GetInputSystem()
{
    return rndr::InputSystem::Get();
}

rndr::InputContext* rndr::RndrContext::GetInputContext()
{
    return rndr::InputSystem::Get()->GetContext();
}

void rndr::RndrContext::Run()
{
    /*real FrameDuration = 0;

    while (!m_Window->IsClosed())
    {
        RNDR_CPU_TRACE("Main Loop");

        auto FrameStart = std::chrono::high_resolution_clock().now();

        m_Window->ProcessEvents();

        if (m_Window->IsWindowMinimized())
        {
            continue;
        }

        rndr::InputSystem::Get()->Update(FrameDuration);

        OnTickDelegate.Execute(FrameDuration);

        m_GraphicsContext->Present(m_SwapChain, false);

        auto FrameEnd = std::chrono::high_resolution_clock().now();
        FrameDuration = std::chrono::duration_cast<std::chrono::microseconds>(FrameEnd - FrameStart).count();
        FrameDuration /= 1'000'000;
        FrameDuration = math::Clamp(FrameDuration, 0, 0.05);
    }*/
}
