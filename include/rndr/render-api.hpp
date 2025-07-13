#pragma once

#include "definitions.hpp"

#if RNDR_OPENGL
#include "rndr/platform/opengl-buffer.hpp"
#include "rndr/platform/opengl-command-list.hpp"
#include "rndr/platform/opengl-frame-buffer.hpp"
#include "rndr/platform/opengl-graphics-context.hpp"
#include "rndr/platform/opengl-pipeline.hpp"
#include "rndr/platform/opengl-shader.hpp"
#include "rndr/platform/opengl-swap-chain.hpp"
#include "rndr/platform/opengl-texture.hpp"
#else
#error "No render API selected."
#endif
