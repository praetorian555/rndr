#pragma once

#include "rndr/definitions.hpp"

#if RNDR_WINDOWS

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#undef near
#undef far
#undef IsMaximized
#undef IsMinimized

#endif // RNDR_WINDOWS
