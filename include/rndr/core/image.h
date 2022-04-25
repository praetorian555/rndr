#pragma once

#include "rndr/core/image.h"

#if defined RDNR_RASTER
#include "rndr/raster/rasterimage.h"
#elif defined RNDR_DX11
#include "rndr/dx11/dx11image.h"
#else
#error Image implementation is missing!
#endif
