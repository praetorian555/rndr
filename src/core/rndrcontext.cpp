#include "rndr/core/rndrcontext.h"

#include <cassert>
#include <chrono>

#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/memory.h"

#include "rndr/profiling/cputracer.h"

#include "rndr/render/graphicscontext.h"
#include "rndr/render/image.h"

static rndr::DefaultAllocator GDefaultAllocator;

rndr::RndrContext* rndr::GRndrContext = nullptr;

rndr::RndrContext::RndrContext(const RndrContextProperties& Props)
{
    if (GRndrContext != nullptr)
    {
        RNDR_LOG_ERROR("RndrContext already created, there can be only one!");
        m_IsInitialized = false;
        return;
    }

    GRndrContext = this;

    m_Props = Props;
    if (Props.UserAllocator != nullptr)
    {
        m_Allocator = Props.UserAllocator;
    }
    else
    {
        m_Allocator = &GDefaultAllocator;
    }
    m_Logger = Props.UserLogger != nullptr ? Props.UserLogger : RNDR_NEW(StdAsyncLogger, "Logger");

    m_InputSystem = RNDR_NEW(InputSystem, "InputSystem");

    m_IsInitialized = true;
}

rndr::RndrContext::~RndrContext()
{
    if (m_IsInitialized)
    {
        RNDR_DELETE(InputSystem, m_InputSystem);

        if (m_Props.UserLogger == nullptr)
        {
            RNDR_DELETE(Logger, m_Logger);
        }
        GRndrContext = nullptr;
    }
}

rndr::ScopePtr<rndr::Window> rndr::RndrContext::CreateWin(int Width,
                                                          int Height,
                                                          const WindowProperties& Props) const
{
    rndr::ScopePtr<rndr::Window> Ptr;

    if (!m_IsInitialized)
    {
        RNDR_LOG_ERROR("RndrContext instance didn't initialize properly!");
        return Ptr;
    }

    Window* W = RNDR_NEW(Window, "rndr::RndrContext: Window");
    if (W == nullptr)
    {
        RNDR_LOG_ERROR("RndrContext::CreateWin: Failed to allocate Window object!");
        return Ptr;
    }
    if (!W->Init(Width, Height, Props))
    {
        RNDR_DELETE(Window, W);
        RNDR_LOG_ERROR("RndrContext::CreateWin: Failed to initialize Window object!");
        return Ptr;
    }
    Ptr.Reset(W);
    return Ptr;
}

rndr::ScopePtr<rndr::GraphicsContext> rndr::RndrContext::CreateGraphicsContext(
    const GraphicsContextProperties& Props) const
{
    ScopePtr<GraphicsContext> Ptr;

    if (!m_IsInitialized)
    {
        RNDR_LOG_ERROR("RndrContext instance didn't initialize properly!");
        return Ptr;
    }

    GraphicsContext* GC = RNDR_NEW(GraphicsContext, "rndr::RndrContext: GraphicsContext");
    if (GC == nullptr || !GC->Init(Props))
    {
        RNDR_DELETE(GraphicsContext, GC);
    }
    Ptr.Reset(GC);
    return Ptr;
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
    if (!m_IsInitialized)
    {
        RNDR_LOG_ERROR("RndrContext instance didn't initialize properly!");
        return nullptr;
    }

    return m_InputSystem;
}

rndr::InputContext* rndr::RndrContext::GetInputContext()
{
    if (!m_IsInitialized)
    {
        RNDR_LOG_ERROR("RndrContext instance didn't initialize properly!");
        return nullptr;
    }

    return m_InputSystem->GetContext();
}

rndr::Allocator* rndr::GetAllocator()
{
    return GRndrContext->GetAllocator();
}
