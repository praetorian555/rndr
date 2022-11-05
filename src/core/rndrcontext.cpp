#include "rndr/core/rndrcontext.h"

#include <cassert>
#include <chrono>

#include "rndr/core/input.h"
#include "rndr/core/rndrcontext.h"
#include "rndr/core/log.h"

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

    m_GraphicsContext = RNDR_NEW(this, GraphicsContext, "rndr::RndrContext: GraphicsContext");
    m_GraphicsContext->Init(this, Props.GraphicsContext);
    if (Props.bCreateWindow)
    {
        m_Window = RNDR_NEW(this, Window, "rndr::RndrContext: Window", Props.WindowWidth, Props.WindowHeight, Props.Window);
        GetInputSystem()->SetWindow(m_Window);

        void* NativeWindowHandle = (void*)m_Window->GetNativeWindowHandle();
        m_SwapChain = m_GraphicsContext->CreateSwapChain(NativeWindowHandle, Props.WindowWidth, Props.WindowHeight, Props.SwapChain);
    }
}

rndr::RndrContext::~RndrContext()
{
    RNDR_DELETE(this, SwapChain, m_SwapChain);
    RNDR_DELETE(this, GraphicsContext, m_GraphicsContext);
    RNDR_DELETE(this, Window, m_Window);

    StdAsyncLogger::Get()->ShutDown();
}

rndr::Window* rndr::RndrContext::GetWindow()
{
    return m_Window;
}

rndr::GraphicsContext* rndr::RndrContext::GetGraphicsContext()
{
    return m_GraphicsContext;
}

rndr::SwapChain* rndr::RndrContext::GetSwapChain()
{
    return m_SwapChain;
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
    real FrameDuration = 0;

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
    }
}
