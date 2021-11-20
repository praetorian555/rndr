#pragma once

#include <cassert>
#include <cstdint>

/**
 * Opaque type that represents an OS window handle.
 */
using NativeWindowHandle = uintptr_t;

/**
 * Opaque type that represents an OS device context;
 */
using NativeDeviceContext = uintptr_t;