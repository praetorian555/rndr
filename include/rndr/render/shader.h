#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/render/raster/rastershader.h"
#elif defined RNDR_DX11
#include "rndr/render/dx11/dx11shader.h"
#else
#error Shader implementation is missing!
#endif
