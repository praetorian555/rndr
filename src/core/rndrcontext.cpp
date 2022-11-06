#include "rndr/core/rndrcontext.h"

#include <cassert>
#include <chrono>

#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/memory.h"
#include "rndr/core/rndrcontext.h"

#include "rndr/profiling/cputracer.h"

#include "rndr/render/graphicscontext.h"
#include "rndr/render/image.h"

static rndr::DefaultAllocator g_DefaultAllocator;

rndr::RndrContext* rndr::GRndrContext = nullptr;

rndr::RndrContext::RndrContext(const RndrContextProperties& Props)
{
    if (GRndrContext)
    {
        RNDR_LOG_ERROR("RndrContext already created, there can be only one!");
        bInitialized = false;
        return;
    }

    GRndrContext = this;

    m_Props = Props;
    if (Props.UserAllocator)
    {
        m_Allocator = Props.UserAllocator;
    }
    else
    {
        m_Allocator = &g_DefaultAllocator;
    }
    m_Logger = Props.UserLogger ? Props.UserLogger : RNDR_NEW(StdAsyncLogger, "Logger");

    bInitialized = true;
}

rndr::RndrContext::~RndrContext()
{
    if (bInitialized)
    {
        if (!m_Props.UserLogger)
        {
            RNDR_DELETE(Logger, m_Logger);
        }
        GRndrContext = nullptr;
    }
}

rndr::Window* rndr::RndrContext::CreateWin(int Width, int Height, const WindowProperties& Props)
{
    if (!bInitialized)
    {
        RNDR_LOG_ERROR("RndrContext instance didn't initialize properly!");
        return nullptr;
    }

    Window* W = RNDR_NEW(Window, "rndr::RndrContext: Window", Width, Height, Props);
    return W;
}

rndr::GraphicsContext* rndr::RndrContext::CreateGraphicsContext(const GraphicsContextProperties& Props)
{
    if (!bInitialized)
    {
        RNDR_LOG_ERROR("RndrContext instance didn't initialize properly!");
        return nullptr;
    }

    GraphicsContext* GC = RNDR_NEW(GraphicsContext, "rndr::RndrContext: GraphicsContext");
    if (!GC || !GC->Init(Props))
    {
        RNDR_DELETE(GraphicsContext, GC);
    }
    return GC;
}

rndr::Logger* rndr::RndrContext::GetLogger()
{
    return m_Logger;
}

rndr::Allocator* rndr::RndrContext::GetAllocator()
{
    return m_Allocator;
}

rndr::InputSystem* rndr::RndrContext::GetInputSystem()
{
    if (!bInitialized)
    {
        RNDR_LOG_ERROR("RndrContext instance didn't initialize properly!");
        return nullptr;
    }

    return rndr::InputSystem::Get();
}

rndr::InputContext* rndr::RndrContext::GetInputContext()
{
    if (!bInitialized)
    {
        RNDR_LOG_ERROR("RndrContext instance didn't initialize properly!");
        return nullptr;
    }

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
