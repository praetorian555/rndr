#pragma once

#include "rndr/core/definitions.h"

#if RNDR_DX11
#include "rndr/render-api/dx11-render-api.h"
#elif RNDR_OPENGL
#include "rndr/render-api/opengl-render-api.h"
#else
#error "No render API selected."
#endif
