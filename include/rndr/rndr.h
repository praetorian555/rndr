#pragma once

#include "opal/container/ref.h"

#include "rndr/definitions.h"
#include "rndr/log.h"
#include "rndr/types.h"

namespace Rndr
{

struct RndrDesc
{
    /** User specified logger. User is responsible for keeping it alive and deallocating it. */
    Opal::Ref<Logger> user_logger;

    /** If we should enable the input system. Defaults to no. */
    bool enable_input_system = false;

    /** If we should enable the CPU tracer. Defaults to no. */
    bool enable_cpu_tracer = false;
};

/**
 * Initializes the Rndr library instance. There can be only one.
 * @param desc Configuration for the library.
 * @return True if the library was initialized successfully.
 */
bool Init(const RndrDesc& desc = {});

/**
 * Destroys the Rndr library instance.
 * @return True if the library was destroyed successfully.
 */
bool Destroy();

/**
 * Waits for the debugger to attach. It will simply loop until the debugger is attached.
 */
void WaitForDebuggerToAttach();

}  // namespace Rndr
