#pragma once

#include "rndr/core/base.h"

#if defined RDNR_RASTER
#include "rndr/render/raster/rastercommandlist.h"
#elif defined RNDR_DX11
#include "rndr/render/dx11/dx11commandlist.h"
#else
#error CommandList implementation is missing!
#endif
