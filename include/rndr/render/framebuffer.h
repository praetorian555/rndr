#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/render/raster/rasterframebuffer.h"
#elif defined RNDR_DX11
#include "rndr/render/dx11/dx11framebuffer.h"
#elif defined RNDR_OPENGL
#include "rndr/render/opengl/opengl-framebuffer.h"
#else
#error Framebuffer implmentation is missing!
#endif
