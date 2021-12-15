#include "rndr/core/debug.h"

#if RNDR_WINDOWS
#include "Windows.h"
#endif  // RNDR_WINDOWS

void rndr_private::DebugBreak()
{
#if RNDR_WINDOWS
    ::DebugBreak();
#else
#error "Platform not supported!"
#endif
}