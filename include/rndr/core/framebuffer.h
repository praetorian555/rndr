#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/raster/rasterframebuffer.h"
#elif defined RNDR_DX11
#include "rndr/dx11/dx11framebuffer.h"
#else
#error Framebuffer implmentation is missing!
#endif
