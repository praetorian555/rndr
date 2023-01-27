#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>

#if WIN32
#define RNDR_WINDOWS 1
#endif  // WIN32

#if !RNDR_WINDOWS
#error "Platform not supported!"
#endif  // !RNDR_WINDOWS

#if RNDR_WINDOWS
#define RNDR_LITTLE_ENDIAN 1
#else
#define RNDR_BIG_ENDIAN 1
#endif  // RNDR_WINDOWS

#if !NDEBUG
#define RNDR_DEBUG 1
#endif  // !NDEBUG

#if !RNDR_RASTER && !RNDR_OPENGL && !RNDR_DX11 && !RNDR_VULKAN && !RNDR_METAL
#define RNDR_RASTER
#endif

#if defined RNDR_DX11
#define RNDR_LEFT_HANDED 1
#else
#define RNDR_RIGHT_HANDED 1
#endif

// Defines precision for floating-point type.
#if !defined(RNDR_REAL_AS_DOUBLE)
using real = float;
#define RNDR_REALC(X) X##f
#else
using real = double;
#define RNDR_REALC(X) X
#endif  // !defined(RNDR_REAL_AS_DOUBLE)

#if RNDR_WINDOWS
#define RNDR_OPTIMIZE_OFF __pragma(optimize("", off))
#define RNDR_OPTIMIZE_ON __pragma(optimize("", on))
#define RNDR_ALIGN(Amount) __declspec(align(Amount))
#endif  // RNDR_WINDOWS

/**
 * Default gamma value.
 */
#define RNDR_GAMMA (2.4)

// Converts methods in the classes to the std::function

#define RNDR_BIND_NO_PARAMS(This, FuncPtr) std::bind(FuncPtr, This)
#define RNDR_BIND_ONE_PARAM(This, FuncPtr) std::bind(FuncPtr, This, std::placeholders::_1)
#define RNDR_BIND_TWO_PARAM(This, FuncPtr) \
    std::bind(FuncPtr, This, std::placeholders::_1, std::placeholders::_2)
#define RNDR_BIND_THREE_PARAM(This, FuncPtr) \
    std::bind(FuncPtr, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
#define RNDR_BIND_FOUR_PARAM(This, FuncPtr)                                                       \
    std::bind(FuncPtr, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, \
              std::placeholders::_4)
#define RNDR_BIND_FIVE_PARAM(This, FuncPtr)                                                       \
    std::bind(FuncPtr, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, \
              std::placeholders::_4, std::placeholders::_5)

#define RNDR_ENUM_TO_TYPE(EnumType, Value) static_cast<std::underlying_type_t<EnumType>>(Value)

#define RNDR_UNUSED(Expr) (void)(Expr)

namespace rndr
{

/**
 * Opaque type that represents an OS window handle.
 */
using NativeWindowHandle = void*;

}  // namespace rndr
