#pragma once

#include "rndr/core/allocator.h"
#include "rndr/core/base.h"
#include "rndr/core/delegate.h"
#include "rndr/core/log.h"
#include "rndr/core/singletons.h"
#include "rndr/core/window.h"

#include "rndr/render/graphicscontext.h"
#include "rndr/render/swapchain.h"

namespace rndr
{

class InputSystem;
class InputContext;

using TickDelegate = MultiDelegate<real /* DeltaSeconds */>;

struct RndrContextProperties
{
    Allocator* UserAllocator = nullptr;
    Logger* UserLogger = nullptr;
};

/**
 * Main API of the rndr library.
 */
class RndrContext
{
public:
    RndrContext(const RndrContextProperties& Props = RndrContextProperties{});
    ~RndrContext();

    Allocator* GetAllocator();
    Logger* GetLogger();

    InputSystem* GetInputSystem();
    InputContext* GetInputContext();

    Window* CreateWin(int Width, int Height, const WindowProperties& Props = WindowProperties{});
    GraphicsContext* CreateGraphicsContext(const GraphicsContextProperties& Props = GraphicsContextProperties{});

    // Run render loop.
    void Run();

public:
    TickDelegate OnTickDelegate;

private:
    RndrContextProperties m_Props;
    bool bInitialized = false;

    Singletons m_Singletons;
    Allocator* m_Allocator = nullptr;
    Logger* m_Logger = nullptr;

    class InputSystem* m_InputSystem = nullptr;
};

extern RndrContext* GRndrContext;

}  // namespace rndr
