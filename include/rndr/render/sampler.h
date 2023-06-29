#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/raster/rastersampler.h"
#elif defined RNDR_DX11
#include "rndr/render/dx11/dx11sampler.h"
#elif defined RNDR_OPENGL
#include "rndr/render/opengl/opengl-sampler.h"
#else
#error Sampler implementation is missing!
#endif