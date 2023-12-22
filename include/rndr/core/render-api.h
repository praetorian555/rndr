#pragma once

#include "rndr/core/definitions.h"

#if RNDR_OPENGL
#include "rndr/core/platform/opengl-render-api.h"
#else
#error "No render API selected."
#endif
