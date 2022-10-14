#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/render/raster/rastergraphicscontext.h"
#elif defined RNDR_DX11
#include "rndr/render/dx11/dx11graphicscontext.h"
#else
#error GraphicsContext implementation is missing!
#endif