#include "rndr/core/rndrapp.h"

#include <cassert>
#include <chrono>

#include "rndr/core/graphicscontext.h"
#include "rndr/core/image.h"
#include "rndr/core/input.h"
#include "rndr/core/rndrapp.h"

#include "rndr/profiling/cputracer.h"

rndr::RndrApp* rndr::GRndrApp = nullptr;

rndr::RndrApp::RndrApp(const RndrAppProperties& Props)
{
    assert(!GRndrApp);
    m_Window = new rndr::Window(Props.WindowWidth, Props.WindowHeight, Props.Window);
    GetInputSystem()->SetWindow(m_Window);
}

rndr::Window* rndr::RndrApp::GetWindow()
{
    return m_Window;
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

        GraphicsContext* GC = m_Window->GetGraphicsContext();
        GC->ClearColor(nullptr, Colors::Pink);
        GC->ClearDepth(nullptr, 1.0);

        OnTickDelegate.Execute(FrameDuration);

        GC->Present(false);

        auto FrameEnd = std::chrono::high_resolution_clock().now();
        FrameDuration = std::chrono::duration_cast<std::chrono::microseconds>(FrameEnd - FrameStart).count();
        FrameDuration /= 1'000'000;
    }
}
