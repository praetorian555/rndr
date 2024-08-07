#pragma once

#include "definitions.h"

#if RNDR_OPENGL
#include "rndr/platform/opengl-buffer.h"
#include "rndr/platform/opengl-command-list.h"
#include "rndr/platform/opengl-frame-buffer.h"
#include "rndr/platform/opengl-graphics-context.h"
#include "rndr/platform/opengl-pipeline.h"
#include "rndr/platform/opengl-shader.h"
#include "rndr/platform/opengl-swap-chain.h"
#include "rndr/platform/opengl-texture.h"
#else
#error "No render API selected."
#endif
