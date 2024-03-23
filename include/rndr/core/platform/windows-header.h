#pragma once

#include "rndr/core/definitions.h"

#if RNDR_WINDOWS

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#undef near
#undef far

#endif // RNDR_WINDOWS
