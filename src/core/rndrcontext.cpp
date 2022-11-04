#include "rndr/core/rndrcontext.h"

#include <cassert>
#include <chrono>

#include "rndr/core/input.h"
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
    m_GraphicsContext = new GraphicsContext();
    m_GraphicsContext->Init();
    if (Props.bCreateWindow)
    {
        m_Window = new rndr::Window(Props.WindowWidth, Props.WindowHeight, Props.Window);
        m_SwapChain = new SwapChain();
        m_SwapChain->Init(m_GraphicsContext, (void*)m_Window->GetNativeWindowHandle(), Props.WindowWidth, Props.WindowHeight);
        GetInputSystem()->SetWindow(m_Window);
    }
}

rndr::RndrContext::~RndrContext()
{
    delete m_SwapChain;
    delete m_GraphicsContext;
    delete m_Window;
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
