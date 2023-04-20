#pragma once

#include "rndr/core/base.h"
#include "rndr/core/forward-def-opengl.h"
#include "rndr/core/graphics-types.h"

#if RNDR_OPENGL

namespace Rndr
{

GLenum FromShaderTypeToOpenGL(ShaderType type);

}

#endif  // RNDR_OPENGL
