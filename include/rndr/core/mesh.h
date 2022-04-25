#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/raster/rastermesh.h"
#elif defined RNDR_DX11
#include "rndr/dx11/dx11mesh.h"
#else
#error Mesh implementation is missing!
#endif