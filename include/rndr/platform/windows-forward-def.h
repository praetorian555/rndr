#pragma once

#include <cstdint>

#include "rndr/definitions.h"

#if RNDR_WINDOWS

#ifndef _WINDEF_

#ifndef RNDR_DECLARE_HANDLE
#define RNDR_DECLARE_HANDLE(name) struct name##__; typedef struct name##__ *name
#endif

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
