#pragma once

#include <cassert>

#include "opal/defines.h"

#if defined(OPAL_PLATFORM_WINDOWS)
#define RNDR_WINDOWS 1
#elif defined(OPAL_PLATFORM_LINUX)
#define RNDR_LINUX 1
#endif

#if defined(OPAL_DEBUG)
#define RNDR_DEBUG 1
#endif

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
#define RNDR_ASSERT(expr, msg) assert(expr&& msg)
#define RNDR_HALT(msg, ...)               \
    do                                    \
    {                                     \
        RNDR_LOG_ERROR(msg, __VA_ARGS__); \
        RNDR_DEBUG_BREAK;                 \
    } while (0)
#else
#define RNDR_STATIC_ASSERT(expr, msg)
#define RNDR_ASSERT(expr, msg)
#define RNDR_HALT(msg, ...) exit(1)
#endif  // RNDR_DEBUG

#define RNDR_UNUSED(Expr) (void)(Expr)

#include <utility>

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

#define RNDR_DECLARE_HANDLE(name) \
    struct name##__;              \
    typedef struct name##__* name

#define RNDR_NOOP
