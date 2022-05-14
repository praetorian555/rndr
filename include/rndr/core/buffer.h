#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/raster/rasterbuffer.h"
#elif defined RNDR_DX11
#include "rndr/dx11/dx11buffer.h"
#else
#error Buffer implementation is missing!
#endif