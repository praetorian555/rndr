#pragma once

#include "rndr/core/definitions.h"

#if RNDR_OPENGL
#include "rndr/core/platform/opengl-graphics-context.h"
#include "rndr/core/platform/opengl-pipeline.h"
#include "rndr/core/platform/opengl-command-list.h"
#include "rndr/core/platform/opengl-image.h"
#include "rndr/core/platform/opengl-buffer.h"
#include "rndr/core/platform/opengl-shader.h"
#include "rndr/core/platform/opengl-swap-chain.h"
#else
#error "No render API selected."
#endif
