#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER
#include "rndr/raster/rastergraphicscontext.h"
#elif defined RNDR_DX11
#include "rndr/dx11/dx11graphicscontext.h"
#else
#error GraphicsContext implementation is missing!
#endif
