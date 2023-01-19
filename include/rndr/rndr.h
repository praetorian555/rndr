#pragma once

// Core utilities
#include "rndr/core/fileutils.h"
#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/memory.h"
#include "rndr/core/projectioncamera.h"
#include "rndr/core/rndrcontext.h"
#include "rndr/core/span.h"
#include "rndr/core/window.h"

// Rendering API
#include "rndr/render/buffer.h"
#include "rndr/render/colors.h"
#include "rndr/render/framebuffer.h"
#include "rndr/render/graphicscontext.h"
#include "rndr/render/graphicstypes.h"
#include "rndr/render/image.h"
#include "rndr/render/pipeline.h"
#include "rndr/render/sampler.h"
#include "rndr/render/shader.h"

// Profiling API
#include "rndr/profiling/cputracer.h"

// Camera API
#include "rndr/camera/firstpersoncamera.h"

// Utility API
#include "rndr/utility/array.h"
#include "rndr/utility/imguiwrapper.h"
#include "rndr/utility/stackarray.h"
