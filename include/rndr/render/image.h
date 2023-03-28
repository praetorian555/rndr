#pragma once

#if defined RDNR_RASTER
#include "rndr/render/raster/rasterimage.h"
#elif defined RNDR_DX11
#include "rndr/render/dx11/dx11image.h"
#elif defined RNDR_OPENGL
#include "rndr/render/opengl/opengl-image.h"
#else
#error Image implementation is missing!
#endif
