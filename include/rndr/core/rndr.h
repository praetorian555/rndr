#pragma once

#include <cassert>
#include <cstdint>

#if WIN32
#define RNDR_WINDOWS 1
#endif  // WIN32

#if !RNDR_WINDOWS
#error "Platform not supported!"
#endif // !RNDR_WINDOWS

#if RNDR_WINDOWS
#define RNDR_LITTLE_ENDIAN 1
#else
#define RNDR_BIG_ENDIAN 1
#endif // RNDR_WINDOWS

#if !NDEBUG
#define RNDR_DEBUG 1
#endif  // !NDEBUG

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
    A8R8G8B8,
    DEPTH_F32
};

/**
 * Defines channels and color space of the channels.
 */
enum class PixelFormat
{
    RGB,
    RGBA,
    sRGB,
    sRGBA,
    DEPTH
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
