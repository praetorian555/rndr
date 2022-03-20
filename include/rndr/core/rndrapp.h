#pragma once

#include "rndr/core/base.h"
#include "rndr/core/delegate.h"
#include "rndr/core/singletons.h"

namespace rndr
{

class Window;
class InputSystem;
class InputContext;

using TickDelegate = MultiDelegate<real /* DeltaSeconds */>;

/**
 * Main API of the rndr library. Each program has only one instance of this class.
 * Instantiated using the RndrInit function call.
 */
class RndrApp
{
public:
    RndrApp();

    Window* GetWindow();
    InputSystem* GetInputSystem();
    InputContext* GetInputContext();

    // Run render loop.
    void Run();

public:
    TickDelegate OnTickDelegate;

private:
    Singletons m_Singletons;
    Window* m_Window;
};

/**
 * Globally available pointer to the only RndrApp instance in this program.
 */
extern RndrApp* GRndrApp;

}  // namespace rndr