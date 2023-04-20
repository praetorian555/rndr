#if RNDR_OPENGL

#include <glad/glad.h>

#include "render-api/opengl-helpers.h"
#include "rndr/core/stack-array.h"

constexpr size_t k_max_shader_type = static_cast<size_t>(Rndr::ShaderType::Max);
constexpr Rndr::StackArray<GLenum, k_max_shader_type> k_to_opengl_shader_type = {
    GL_VERTEX_SHADER,
    GL_FRAGMENT_SHADER,
    GL_GEOMETRY_SHADER,
    GL_TESS_CONTROL_SHADER,
    GL_TESS_EVALUATION_SHADER,
    GL_COMPUTE_SHADER};

GLenum Rndr::FromShaderTypeToOpenGL(ShaderType type)
{
    return k_to_opengl_shader_type[static_cast<size_t>(type)];
}

#endif  // RNDR_OPENGL
