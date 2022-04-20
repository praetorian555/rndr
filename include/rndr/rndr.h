#pragma once

#include "rndr/core/projectioncamera.h"
#include "rndr/core/colors.h"
#include "rndr/core/fileutils.h"
#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/model.h"
#include "rndr/core/pipeline.h"
#include "rndr/core/rndrapp.h"
#include "rndr/core/setup.h"
#include "rndr/core/singletons.h"
#include "rndr/core/span.h"
#include "rndr/core/threading.h"
#include "rndr/core/transform.h"
#include "rndr/core/window.h"

#include "rndr/geometry/cube.h"

#include "rndr/profiling/cputracer.h"

#include "rndr/raster/rasterframebuffer.h"
#include "rndr/raster/rastergraphicscontext.h"
#include "rndr/raster/rasterimage.h"
#include "rndr/raster/rasterizer.h"
#include "rndr/raster/rasterpipeline.h"
#include "rndr/raster/shaders/phong.h"

#include "rndr/camera/firstpersoncamera.h"
