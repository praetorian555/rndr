#if RNDR_OPENGL

#include <glad/glad.h>

#include "render-api/opengl-helpers.h"
#include "rndr/core/stack-array.h"

constexpr size_t k_max_shader_type = static_cast<size_t>(Rndr::ShaderType::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_shader_type> k_to_opengl_shader_type = {
    GL_VERTEX_SHADER,
    GL_FRAGMENT_SHADER,
    GL_GEOMETRY_SHADER,
    GL_TESS_CONTROL_SHADER,
    GL_TESS_EVALUATION_SHADER,
    GL_COMPUTE_SHADER};

constexpr size_t k_max_usage = static_cast<size_t>(Rndr::Usage::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_usage> k_to_opengl_usage = {GL_MAP_WRITE_BIT,
                                                                     GL_DYNAMIC_STORAGE_BIT,
                                                                     GL_MAP_READ_BIT};

constexpr size_t k_max_buffer_type = static_cast<size_t>(Rndr::BufferType::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_buffer_type> k_to_opengl_buffer_type = {
    GL_ARRAY_BUFFER,
    GL_ELEMENT_ARRAY_BUFFER,
    GL_UNIFORM_BUFFER};

constexpr size_t k_max_comparator = static_cast<size_t>(Rndr::Comparator::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_comparator> k_to_opengl_comparator =
    {GL_NEVER, GL_ALWAYS, GL_LESS, GL_GREATER, GL_EQUAL, GL_NOTEQUAL, GL_LEQUAL, GL_GEQUAL};

constexpr size_t k_max_stencil_op = static_cast<size_t>(Rndr::StencilOperation::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_stencil_op> k_to_opengl_stencil_op =
    {GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_INCR_WRAP, GL_DECR, GL_DECR_WRAP, GL_INVERT};

constexpr size_t k_max_blend_factor = static_cast<size_t>(Rndr::BlendFactor::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_blend_factor> k_to_opengl_blend_factor = {
    GL_ZERO,
    GL_ONE,
    GL_SRC_COLOR,
    GL_DST_COLOR,
    GL_ONE_MINUS_SRC_COLOR,
    GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA,
    GL_DST_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_ONE_MINUS_DST_ALPHA,
    GL_CONSTANT_COLOR,
    GL_ONE_MINUS_CONSTANT_COLOR,
    GL_CONSTANT_ALPHA,
    GL_ONE_MINUS_CONSTANT_ALPHA};

constexpr size_t k_max_blend_op = static_cast<size_t>(Rndr::BlendOperation::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_blend_op> k_to_opengl_blend_op = {GL_FUNC_ADD,
                                                                           GL_FUNC_SUBTRACT,
                                                                           GL_FUNC_REVERSE_SUBTRACT,
                                                                           GL_MIN,
                                                                           GL_MAX};

GLenum Rndr::FromShaderTypeToOpenGL(ShaderType type)
{
    return k_to_opengl_shader_type[static_cast<size_t>(type)];
}

GLenum Rndr::FromUsageToOpenGL(Usage usage)
{
    return k_to_opengl_usage[static_cast<size_t>(usage)];
}

GLenum Rndr::FromBufferTypeToOpenGL(BufferType type)
{
    return k_to_opengl_buffer_type[static_cast<size_t>(type)];
}

GLenum Rndr::FromComparatorToOpenGL(Comparator comparator)
{
    return k_to_opengl_comparator[static_cast<size_t>(comparator)];
}

GLenum Rndr::FromStencilOpToOpenGL(StencilOperation op)
{
    return k_to_opengl_stencil_op[static_cast<size_t>(op)];
}

GLenum Rndr::FromBlendFactorToOpenGL(BlendFactor factor)
{
    return k_to_opengl_blend_factor[static_cast<size_t>(factor)];
}

GLenum Rndr::FromBlendOperationToOpenGL(BlendOperation op)
{
    return k_to_opengl_blend_op[static_cast<size_t>(op)];
}

#endif  // RNDR_OPENGL
