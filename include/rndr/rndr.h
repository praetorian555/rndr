#pragma once

#include <cassert>
#include <cstdint>

/**
 * Opaque type that represents an OS window handle.
 */
using NativeWindowHandle = uintptr_t;

// Defines precision for floating-point type.
#if !defined(RNDR_REAL_AS_DOUBLE)
using real = float;
#else
using real = double;
#endif  // !defined(RNDR_REAL_AS_DOUBLE)