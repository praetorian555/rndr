#pragma once

#include <cstdint>

#include "rndr/core/definitions.h"

#if RNDR_WINDOWS

#ifndef _WINDEF_

#define RNDR_DECLARE_HANDLE(name) struct name##__; typedef struct name##__ *name
RNDR_DECLARE_HANDLE(HWND);
RNDR_DECLARE_HANDLE(HDC);
RNDR_DECLARE_HANDLE(HGLRC);

using UINT = unsigned int;
using WPARAM = uint64_t;
using LPARAM = __int64;
using LRESULT = __int64;

#define CALLBACK __stdcall

#endif  // _WINDEF_

namespace Rndr
{

/**
 * Represents a native window handle. Underlying type changes with the OS.
 */
using NativeWindowHandle = HWND;

/**
 * Represents an invalid window handle.
 */
constexpr NativeWindowHandle k_invalid_window_handle = NULL;

/**
 * Represents a native device context handle. Underlying type changes with the OS.
 */
using NativeDeviceContextHandle = HDC;

/**
 * Represents an invalid device context handle.
 */
constexpr NativeDeviceContextHandle k_invalid_device_context_handle = NULL;

/**
 * Represents a native graphics context handle. Underlying type changes with the OS.
 */
using NativeGraphicsContextHandle = HGLRC;

/**
 * Represents an invalid graphics context handle.
 */
constexpr NativeGraphicsContextHandle k_invalid_graphics_context_handle = NULL;

}  // namespace Rndr

#endif  // RNDR_WINDOWS
