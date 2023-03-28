#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/render/raster/rasterpipeline.h"
#elif defined RNDR_DX11
#include "rndr/render/dx11/dx11pipeline.h"
#elif defined RNDR_OPENGL
#include "rndr/render/opengl/opengl-pipeline.h"
#else
#error Pipeline implementation is missing!
#endif
