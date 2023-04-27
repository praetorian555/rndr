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

constexpr size_t k_max_image_address_mode = static_cast<size_t>(Rndr::ImageAddressMode::EnumCount);
constexpr Rndr::StackArray<GLint, k_max_image_address_mode> k_to_opengl_image_address_mode =
    {GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_REPEAT, GL_MIRRORED_REPEAT, GL_MIRROR_CLAMP_TO_EDGE};

constexpr size_t k_max_pixel_format = static_cast<size_t>(Rndr::PixelFormat::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_pixel_format> k_to_opengl_internal_pixel_format = {
    GL_RGBA8,
    GL_RGBA8,
    GL_RGBA8UI,
    GL_RGBA8_SNORM,
    GL_RGBA8I,
    GL_RGBA8,
    GL_RGBA8,

    GL_DEPTH24_STENCIL8,

    GL_R8,
    GL_R8UI,
    GL_R8_SNORM,
    GL_R8I,

    GL_RGBA32F,
    GL_RGBA32UI,
    GL_RGBA32I,

    GL_RGB32F,
    GL_RGB32UI,
    GL_RGB32I,

    GL_RG32F,
    GL_RG32UI,
    GL_RG32I,

    GL_R32F,
    GL_R32UI,
    GL_R32I,

    GL_R32F,
    GL_R16F};
constexpr Rndr::StackArray<GLenum, k_max_pixel_format> k_to_opengl_pixel_format = {GL_RGBA,
                                                                                   GL_RGBA,
                                                                                   GL_RGBA,
                                                                                   GL_RGBA,
                                                                                   GL_RGBA,
                                                                                   GL_BGRA,
                                                                                   GL_BGRA,

                                                                                   GL_DEPTH_STENCIL,

                                                                                   GL_RED,
                                                                                   GL_RED,
                                                                                   GL_RED,
                                                                                   GL_RED,

                                                                                   GL_RGBA,
                                                                                   GL_RGBA,
                                                                                   GL_RGBA,

                                                                                   GL_RGB,
                                                                                   GL_RGB,
                                                                                   GL_RGB,

                                                                                   GL_RG,
                                                                                   GL_RG,
                                                                                   GL_RG,

                                                                                   GL_RED,
                                                                                   GL_RED,
                                                                                   GL_RED,

                                                                                   GL_RED,
                                                                                   GL_RED};
constexpr Rndr::StackArray<GLenum, k_max_pixel_format> k_to_opengl_pixel_type = {
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_BYTE,
    GL_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,

    GL_UNSIGNED_INT_24_8,

    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_BYTE,
    GL_BYTE,

    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,

    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,

    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,

    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,

    GL_FLOAT,
    GL_UNSIGNED_SHORT};
constexpr Rndr::StackArray<int32_t, k_max_pixel_format> k_to_pixel_size = {
    4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1, 16, 16, 16, 12, 12, 12, 8, 8, 8, 4, 4, 4, 4, 2};

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

GLenum Rndr::FromImageInfoToTarget(ImageType image_type, bool use_mips)
{
    GLenum target = GL_TEXTURE_2D;
    switch (image_type)
    {
        case ImageType::Image2D:
            target = use_mips ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
            break;
        case ImageType::Image2DArray:
            target = use_mips ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
            break;
        case ImageType::CubeMap:
            target = GL_TEXTURE_CUBE_MAP;
            break;
        default:
            assert(false && "Unsupported image type");
            break;
    }

    return target;
}

GLint Rndr::FromImageFilterToMinFilter(ImageFilter min_filter, ImageFilter mip_filter)
{
    GLint gl_filter = GL_LINEAR;
    switch (min_filter)
    {
        case ImageFilter::Nearest:
            gl_filter = mip_filter == ImageFilter::Nearest ? GL_NEAREST_MIPMAP_NEAREST
                                                           : GL_NEAREST_MIPMAP_LINEAR;
            break;
        case ImageFilter::Linear:
            gl_filter = mip_filter == ImageFilter::Nearest ? GL_LINEAR_MIPMAP_NEAREST
                                                           : GL_LINEAR_MIPMAP_LINEAR;
            break;
        default:
            assert(false && "Unsupported image filter");
            break;
    }

    return gl_filter;
}

GLint Rndr::FromImageFilterToOpenGL(ImageFilter filter)
{
    GLint gl_filter = GL_LINEAR;
    switch (filter)
    {
        case ImageFilter::Nearest:
            gl_filter = GL_NEAREST;
            break;
        case ImageFilter::Linear:
            gl_filter = GL_LINEAR;
            break;
        default:
            assert(false && "Unsupported image filter");
            break;
    }

    return gl_filter;
}

GLint Rndr::FromImageAddressModeToOpenGL(ImageAddressMode address_mode)
{
    return k_to_opengl_image_address_mode[static_cast<size_t>(address_mode)];
}

GLenum Rndr::FromPixelFormatToInternalFormat(PixelFormat format)
{
    return k_to_opengl_internal_pixel_format[static_cast<size_t>(format)];
}

GLenum Rndr::FromPixelFormatToFormat(PixelFormat format)
{
    return k_to_opengl_pixel_format[static_cast<size_t>(format)];
}

GLenum Rndr::FromPixelFormatToDataType(PixelFormat format)
{
    return k_to_opengl_pixel_type[static_cast<size_t>(format)];
}

int32_t Rndr::GetPixelSize(PixelFormat format)
{
    return k_to_pixel_size[static_cast<size_t>(format)];
}

#endif  // RNDR_OPENGL
