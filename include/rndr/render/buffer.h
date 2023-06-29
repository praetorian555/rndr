#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/render/raster/rasterbuffer.h"
#elif defined RNDR_DX11
#include "rndr/render/dx11/dx11buffer.h"
#elif defined RNDR_OPENGL
#include "rndr/render/opengl/opengl-buffer.h"
#else
#error Buffer implementation is missing!
#endif