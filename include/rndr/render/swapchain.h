#pragma once

#include "rndr/core/base.h"

#if RNDR_DX11
#include "rndr/render/dx11/dx11swapchain.h"
#elif RNDR_RASTER
#error "Swapchain missing!"
#endif