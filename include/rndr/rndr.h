#pragma once

// Core utilities
#include "rndr/core/base.h"
#include "rndr/core/colors.h"
#include "opal/container/array.h"
#include "rndr/core/containers/hash-map.h"
#include "rndr/core/containers/ref.h"
#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/span.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/definitions.h"
#include "rndr/core/delegate.h"
#include "rndr/core/file.h"
#include "rndr/core/graphics-types.h"
#include "rndr/core/input-primitives.h"
#include "rndr/core/input.h"
#include "rndr/core/math.h"
#include "rndr/core/projection-camera.h"
#include "rndr/core/render-api.h"
#include "rndr/core/renderer-base.h"
#include "rndr/core/time.h"
#include "rndr/core/window.h"
#include "rndr/utility/cpu-tracer.h"
#include "rndr/utility/cube-map.h"
#include "rndr/utility/fly-camera.h"
#include "rndr/utility/frames-per-second-counter.h"
#include "rndr/utility/imgui-wrapper.h"
#include "rndr/utility/input-layout-builder.h"
#include "rndr/utility/line-renderer.h"
#include "rndr/utility/material.h"
#include "rndr/utility/mesh.h"
#include "rndr/utility/scene.h"
