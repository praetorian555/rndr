#include "rndr/core/rndrapp.h"

#include <cassert>
#include <chrono>

#include "rndr/core/input.h"
#include "rndr/core/rndrapp.h"

#include "rndr/profiling/cputracer.h"

#include "rndr/render/graphicscontext.h"
#include "rndr/render/image.h"

rndr::RndrApp* rndr::GRndrApp = nullptr;

rndr::RndrApp::RndrApp(const RndrAppProperties& Props)
{
    assert(!GRndrApp);
    m_Window = new rndr::Window(Props.WindowWidth, Props.WindowHeight, Props.Window);
    m_GraphicsContext = new GraphicsContext();
    m_GraphicsContext->Init();
    m_SwapChain = new SwapChain();
    m_SwapChain->Init(m_GraphicsContext, (void*)m_Window->GetNativeWindowHandle(), Props.WindowWidth, Props.WindowHeight);
    GetInputSystem()->SetWindow(m_Window);
}

rndr::Window* rndr::RndrApp::GetWindow()
{
    return m_Window;
}

rndr::GraphicsContext* rndr::RndrApp::GetGraphicsContext()
{
    return m_GraphicsContext;
}

rndr::SwapChain* rndr::RndrApp::GetSwapChain()
{
    return m_SwapChain;
}

rndr::InputSystem* rndr::RndrApp::GetInputSystem()
{
    return rndr::InputSystem::Get();
}

rndr::InputContext* rndr::RndrApp::GetInputContext()
{
    return rndr::InputSystem::Get()->GetContext();
}

void rndr::RndrApp::Run()
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
