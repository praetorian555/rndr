#pragma once

#include <cassert>
#include <cstdint>

#if !NDEBUG
#define RNDR_DEBUG 1
#endif  // !NDEBUG

#if WIN32
#define RNDR_WINDOWS 1
#endif  // WIN32

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
 * Defines channels and color space of the channels.
 */
enum class PixelFormat
{
    RGB,
    RGBA,
    sRGB,
    sRGBA
};

/**
 * Enumerates all image file formats supported by the renderer.
 */
enum class ImageFileFormat
{
    BMP = 0,
    NotSupported
};

/**
 * Opaque type that represents an OS window handle.
 */
using NativeWindowHandle = uintptr_t;

}  // namespace rndr
