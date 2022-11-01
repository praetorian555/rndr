#pragma once

#include "rndr/core/base.h"
#include "rndr/core/delegate.h"
#include "rndr/core/singletons.h"
#include "rndr/core/window.h"

#include "rndr/render/graphicscontext.h"
#include "rndr/render/swapchain.h"

namespace rndr
{

class InputSystem;
class InputContext;

using TickDelegate = MultiDelegate<real /* DeltaSeconds */>;

struct RndrAppProperties
{
    int WindowWidth = 1024;
    int WindowHeight = 768;

    WindowProperties Window;
};

/**
 * Main API of the rndr library. Each program has only one instance of this class.
 * Instantiated using the RndrInit function call.
 */
class RndrApp
{
public:
    RndrApp(const RndrAppProperties& Props = RndrAppProperties{});

    Window* GetWindow();
    GraphicsContext* GetGraphicsContext();
    SwapChain* GetSwapChain();
    InputSystem* GetInputSystem();
    InputContext* GetInputContext();

    // Run render loop.
    void Run();

public:
    TickDelegate OnTickDelegate;

private:
    Singletons m_Singletons;
    Window* m_Window;
    GraphicsContext* m_GraphicsContext;
    SwapChain* m_SwapChain;
};

/**
 * Globally available pointer to the only RndrApp instance in this program.
 */
extern RndrApp* GRndrApp;

}  // namespace rndr