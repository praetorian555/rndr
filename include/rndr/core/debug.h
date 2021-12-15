#pragma once

#include "rndr/core/rndr.h"

namespace rndr_private
{
void DebugBreak();
}

#if RNDR_DEBUG

/**
 * Triggers a breakpoint when the Condition is true.
 */
#define RNDR_COND_BP(Condition)     \
    if (Condition)                  \
    {                               \
        rndr_private::DebugBreak(); \
    }

#else

#define RNDR_COND_BP(Condition)

#endif  // RNDR_DEBUG