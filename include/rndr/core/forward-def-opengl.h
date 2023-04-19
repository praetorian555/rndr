#pragma once

#include "rndr/core/definitions.h"

#if RNDR_OPENGL

using GLuint = unsigned int;

namespace Rndr
{

/**
 * Represents an invalid OpenGL object.
 */
constexpr GLuint k_invalid_opengl_object = 0;

}

#endif // RNDR_OPENGL