#pragma once

#include <cassert>
#include <cstdint>

namespace rndr_private
{
void DebugBreak();
}

#define RNDR_COND_BP(cond)          \
    if (cond)                       \
    {                               \
        rndr_private::DebugBreak(); \
    }

// Defines precision for floating-point type.
#if !defined(RNDR_REAL_AS_DOUBLE)
using real = float;
#else
using real = double;
#endif  // !defined(RNDR_REAL_AS_DOUBLE)

namespace rndr
{

/**
 * Exact positions of channels and their size in bits.
 */
enum class PixelLayout
{
    A8R8G8B8
};

/**
 * Opaque type that represents an OS window handle.
 */
using NativeWindowHandle = uintptr_t;

// Functions

real ToGammaCorrectSpace(real Value, real Gamma = 2.4);
real ToLinearSpace(real Value, real Gamma = 2.4);

/**
 * Get size of a pixel in bytes.
 */
int GetPixelSize(PixelLayout Layout);

}  // namespace rndr
