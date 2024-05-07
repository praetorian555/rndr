#pragma once

#include <cassert>

#if WIN32
#define RNDR_WINDOWS 1
#endif  // WIN32

#if !RNDR_WINDOWS
#error "Platform not supported!"
#endif  // !RNDR_WINDOWS

#if !NDEBUG
#define RNDR_DEBUG 1
#endif  // !NDEBUG

#if RNDR_WINDOWS
#define RNDR_OPTIMIZE_OFF __pragma(optimize("", off))
#define RNDR_OPTIMIZE_ON __pragma(optimize("", on))
#define RNDR_ALIGN(Amount) __declspec(align(Amount))
#define RNDR_FORCE_INLINE __forceinline
#define RNDR_DEBUG_BREAK __debugbreak()
#else
#error "Platfrom not supported!"
#endif  // RNDR_WINDOWS

#if RNDR_DEBUG
#define RNDR_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
#define RNDR_ASSERT(expr) do { if (!(expr)) { RNDR_DEBUG_BREAK; } } while (0)
#define RNDR_HALT(msg) do { RNDR_DEBUG_BREAK; } while (0)
#else
#define RNDR_STATIC_ASSERT(expr, msg)
#define RNDR_ASSERT(expr)
#define RNDR_HALT(msg) exit(1)
#endif  // RNDR_DEBUG

#define RNDR_UNUSED(Expr) (void)(Expr)

#include "rndr/export.h"

// disable warnings on extern before template instantiation
#pragma warning(disable : 4231)

#if defined(RNDR_WINDOWS)
#if rndr_EXPORTS
#define RNDR_EXTERN_TEMPLATE
#else
#define RNDR_EXTERN_TEMPLATE extern
#endif
#endif
