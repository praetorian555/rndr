#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>

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

// Defines precision for floating-point type.
#if !defined(RNDR_REAL_AS_DOUBLE)
using real = float;
#else
using real = double;
#endif  // !defined(RNDR_REAL_AS_DOUBLE)

#if RNDR_WINDOWS
#define RNDR_OPTIMIZE_OFF __pragma(optimize("", off))
#define RNDR_OPTIMIZE_ON __pragma(optimize("", on))
#endif  // RNDR_WINDOWS

namespace rndr
{

/**
 * Exact positions of channels and their size in bits.
 */
enum class PixelLayout
{
    A8R8G8B8,
    B8G8R8A8,
    R8G8B8A8,
    DEPTH_F32
};

/**
 * Specifies the space in which the color is in the gamma correction.
 */
enum class GammaSpace
{
    GammaCorrected,
    Linear
};

/**
 * Enumerates all image file formats supported by the renderer.
 */
enum class ImageFileFormat
{
    BMP = 0,
    PNG,
    JPEG,
    NotSupported
};

/**
 * Supported filtering techniques used for minification and magnification of an image.
 */
enum class ImageFiltering
{
    NearestNeighbor,
    BilinearInterpolation,
    TrilinearInterpolation  // Valid only for minification filter
};

/**
 * Supported depth test operations.
 */
enum class DepthTest
{
    None,
    GreaterThan,
    LesserThen
};

enum class WindingOrder : int
{
    CW = -1,
    CCW = 1
};

/**
 * Opaque type that represents an OS window handle.
 */
using NativeWindowHandle = uintptr_t;

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

}  // namespace rndr
