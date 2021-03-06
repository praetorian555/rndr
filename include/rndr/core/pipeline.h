#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/raster/rasterpipeline.h"
#elif defined RNDR_DX11
#include "rndr/dx11/dx11pipeline.h"
#else
#error Pipeline implementation is missing!
#endif
