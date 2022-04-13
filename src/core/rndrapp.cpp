#include "rndr/core/rndrapp.h"

#include <cassert>
#include <chrono>

#include "rndr/core/input.h"
#include "rndr/core/rndrapp.h"
#include "rndr/core/window.h"

#include "rndr/profiling/cputracer.h"

rndr::RndrApp* rndr::GRndrApp = nullptr;

rndr::RndrApp::RndrApp()
{
    assert(!GRndrApp);
    m_Window = new rndr::Window{};
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

        rndr::Image* ColorImage = m_Window->GetColorImage();
        rndr::Image* DepthImage = m_Window->GetDepthImage();
        ColorImage->ClearColorBuffer(rndr::Color::Black);
        DepthImage->ClearDepthBuffer(rndr::Infinity);

        OnTickDelegate.Execute(FrameDuration);

        m_Window->RenderToWindow();

        auto FrameEnd = std::chrono::high_resolution_clock().now();
        FrameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(FrameEnd - FrameStart).count();
        FrameDuration /= 1000;
    }
}
