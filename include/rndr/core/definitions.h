#pragma once

#include <cassert>
#include <cstdint>

#include "math/math.h"

#if WIN32
#define RNDR_WINDOWS 1
#endif  // WIN32

#if !RNDR_WINDOWS
#error "Platform not supported!"
#endif  // !RNDR_WINDOWS

#if !NDEBUG
#define RNDR_DEBUG 1
#endif  // !NDEBUG

namespace Rndr
{
using real = math::real;
} // namespace Rndr

#if RNDR_WINDOWS
#define RNDR_OPTIMIZE_OFF __pragma(optimize("", off))
#define RNDR_OPTIMIZE_ON __pragma(optimize("", on))
#define RNDR_ALIGN(Amount) __declspec(align(Amount))
#endif  // RNDR_WINDOWS

#define RNDR_UNUSED(Expr) (void)(Expr)

#define RNDR_NO_CONSTRCUTORS_AND_DESTRUCTOR(class) \
    class() = default;                             \
    ~class() = default;                            \
    class(const class&) = delete;                  \
    class(class&&) = delete;                       \
    class& operator=(const class&) = delete;       \
    class& operator=(class&&) = delete;
