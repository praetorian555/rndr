#if RNDR_OPENGL

#include <glad/glad.h>

#include "render-api/opengl-helpers.h"
#include "rndr/core/stack-array.h"
#include "rndr/core/graphics-types.h"

constexpr size_t k_max_shader_type = static_cast<size_t>(Rndr::ShaderType::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_shader_type> k_to_opengl_shader_type = {
    GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_COMPUTE_SHADER};

constexpr size_t k_max_usage = static_cast<size_t>(Rndr::Usage::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_usage> k_to_opengl_usage = {GL_MAP_WRITE_BIT, GL_DYNAMIC_STORAGE_BIT, GL_MAP_READ_BIT};

constexpr size_t k_max_buffer_type = static_cast<size_t>(Rndr::BufferType::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_buffer_type> k_to_opengl_buffer_type = {GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER,
                                                                                 GL_UNIFORM_BUFFER, GL_SHADER_STORAGE_BUFFER};


constexpr size_t k_max_comparator = static_cast<size_t>(Rndr::Comparator::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_comparator> k_to_opengl_comparator = {GL_NEVER, GL_ALWAYS,   GL_LESS,   GL_GREATER,
                                                                               GL_EQUAL, GL_NOTEQUAL, GL_LEQUAL, GL_GEQUAL};

constexpr size_t k_max_stencil_op = static_cast<size_t>(Rndr::StencilOperation::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_stencil_op> k_to_opengl_stencil_op = {GL_KEEP,      GL_ZERO, GL_REPLACE,   GL_INCR,
                                                                               GL_INCR_WRAP, GL_DECR, GL_DECR_WRAP, GL_INVERT};

constexpr size_t k_max_blend_factor = static_cast<size_t>(Rndr::BlendFactor::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_blend_factor> k_to_opengl_blend_factor = {GL_ZERO,
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
constexpr Rndr::StackArray<GLenum, k_max_blend_op> k_to_opengl_blend_op = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN,
                                                                           GL_MAX};

constexpr size_t k_max_image_address_mode = static_cast<size_t>(Rndr::ImageAddressMode::EnumCount);
constexpr Rndr::StackArray<GLint, k_max_image_address_mode> k_to_opengl_image_address_mode = {
    GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_REPEAT, GL_MIRRORED_REPEAT, GL_MIRROR_CLAMP_TO_EDGE};

constexpr size_t k_max_pixel_format = static_cast<size_t>(Rndr::PixelFormat::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_pixel_format> k_to_opengl_internal_pixel_format = {GL_RGBA8,
                                                                                            GL_SRGB8_ALPHA8,
                                                                                            GL_RGBA8UI,
                                                                                            GL_RGBA8_SNORM,
                                                                                            GL_RGBA8I,
                                                                                            GL_RGBA8,
                                                                                            GL_SRGB8_ALPHA8,

                                                                                            GL_DEPTH24_STENCIL8,

                                                                                            GL_RGB8,
                                                                                            GL_SRGB8,
                                                                                            GL_RGB8UI,
                                                                                            GL_RGB8_SNORM,
                                                                                            GL_RGB8I,

                                                                                            GL_RG8,
                                                                                            GL_RG8,
                                                                                            GL_RG8UI,
                                                                                            GL_RG8_SNORM,
                                                                                            GL_RG8I,

                                                                                            GL_R8,
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

                                                                                   GL_RGB,
                                                                                   GL_RGB,
                                                                                   GL_RGB,
                                                                                   GL_RGB,
                                                                                   GL_RGB,

                                                                                   GL_RG,
                                                                                   GL_RG,
                                                                                   GL_RG,
                                                                                   GL_RG,
                                                                                   GL_RG,

                                                                                   GL_RED,
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
constexpr Rndr::StackArray<GLenum, k_max_pixel_format> k_to_opengl_pixel_type = {GL_UNSIGNED_BYTE,
                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_BYTE,
                                                                                 GL_BYTE,
                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_UNSIGNED_BYTE,

                                                                                 GL_UNSIGNED_INT_24_8,

                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_BYTE,
                                                                                 GL_BYTE,

                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_BYTE,
                                                                                 GL_BYTE,

                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_UNSIGNED_BYTE,
                                                                                 GL_BYTE,
                                                                                 GL_BYTE,

                                                                                 GL_FLOAT,
                                                                                 GL_UNSIGNED_INT,
                                                                                 GL_INT,

                                                                                 GL_FLOAT,
                                                                                 GL_UNSIGNED_INT,
                                                                                 GL_INT,

                                                                                 GL_FLOAT,
                                                                                 GL_UNSIGNED_INT,
                                                                                 GL_INT,

                                                                                 GL_FLOAT,
                                                                                 GL_UNSIGNED_INT,
                                                                                 GL_INT,

                                                                                 GL_FLOAT,
                                                                                 GL_HALF_FLOAT};
constexpr Rndr::StackArray<int32_t, k_max_pixel_format> k_to_pixel_size = {4, 4, 4, 4, 4,  4,  4,  4,  3,  3,  3, 3, 3, 2, 2, 2, 2, 2, 1,
                                                                           1, 1, 1, 1, 16, 16, 16, 12, 12, 12, 8, 8, 8, 4, 4, 4, 4, 2};
constexpr Rndr::StackArray<GLint, k_max_pixel_format> k_to_component_count = {4, 4, 4, 4, 4, 4, 4, 2, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1,
                                                                              1, 1, 1, 1, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1};
constexpr Rndr::StackArray<GLenum, k_max_pixel_format> k_to_should_normalize_data = {
    GL_TRUE,  GL_TRUE,  GL_FALSE, GL_TRUE,  GL_FALSE, GL_TRUE,  GL_TRUE,  GL_FALSE, GL_TRUE,  GL_TRUE,  GL_FALSE, GL_TRUE,  GL_FALSE,
    GL_TRUE,  GL_TRUE,  GL_FALSE, GL_TRUE,  GL_FALSE, GL_TRUE,  GL_TRUE,  GL_FALSE, GL_TRUE,  GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE,
    GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE};

constexpr size_t k_max_primitive_topology = static_cast<size_t>(Rndr::PrimitiveTopology::EnumCount);
constexpr Rndr::StackArray<GLenum, k_max_primitive_topology> k_to_opengl_primitive_topology = {GL_POINTS, GL_LINES, GL_LINE_STRIP,
                                                                                               GL_TRIANGLES, GL_TRIANGLE_STRIP};

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

GLenum Rndr::FromImageInfoToTarget(ImageType image_type, bool is_multi_sample)
{
    GLenum target = GL_TEXTURE_2D;
    switch (image_type)
    {
        case ImageType::Image2D:
            target = is_multi_sample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
            break;
        case ImageType::Image2DArray:
            target = is_multi_sample ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
            break;
        case ImageType::CubeMap:
            target = GL_TEXTURE_CUBE_MAP;
            break;
        default:
            RNDR_HALT("Unsupported image type");
            break;
    }

    return target;
}

GLint Rndr::FromMinAndMipFiltersToOpenGL(ImageFilter min_filter, ImageFilter mip_filter)
{
    GLint gl_filter = GL_LINEAR;
    switch (min_filter)
    {
        case ImageFilter::Nearest:
            gl_filter = mip_filter == ImageFilter::Nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
            break;
        case ImageFilter::Linear:
            gl_filter = mip_filter == ImageFilter::Nearest ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
            break;
        default:
            RNDR_HALT("Unsupported image filter");
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
            RNDR_HALT("Unsupported image filter");
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

int32_t Rndr::FromPixelFormatToPixelSize(PixelFormat format)
{
    return k_to_pixel_size[static_cast<size_t>(format)];
}

GLint Rndr::FromPixelFormatToComponentCount(PixelFormat format)
{
    return k_to_component_count[static_cast<size_t>(format)];
}

GLenum Rndr::FromPixelFormatToShouldNormalizeData(Rndr::PixelFormat format)
{
    return k_to_should_normalize_data[static_cast<size_t>(format)];
}

GLenum Rndr::FromPrimitiveTopologyToOpenGL(PrimitiveTopology topology)
{
    return k_to_opengl_primitive_topology[static_cast<size_t>(topology)];
}

GLenum Rndr::FromIndexSizeToOpenGL(uint32_t index_size)
{
    switch (index_size)
    {
        case 1:
            return GL_UNSIGNED_BYTE;
        case 2:
            return GL_UNSIGNED_SHORT;
        case 4:
            return GL_UNSIGNED_INT;
        default:
            RNDR_HALT("Unsupported index size");
            return GL_UNSIGNED_INT;
    }
}

bool Rndr::IsComponentLowPrecision(PixelFormat format)
{
    const GLenum data_type = FromPixelFormatToDataType(format);
    return data_type == GL_UNSIGNED_BYTE || data_type == GL_BYTE;
}

bool Rndr::IsComponentHighPrecision(PixelFormat format)
{
    const GLenum data_type = FromPixelFormatToDataType(format);
    return data_type == GL_FLOAT;
}

Rndr::String Rndr::FromBufferTypeToString(Rndr::BufferType type)
{
    switch (type)
    {
        case BufferType::Vertex:
            return "Vertex";
        case BufferType::Index:
            return "Index";
        case BufferType::Constant:
            return "Constant";
        case BufferType::ShaderStorage:
            return "Storage";
        default:
            RNDR_HALT("Invalid buffer type");
    }
    return "";
}

Rndr::String Rndr::FromOpenGLDataTypeToString(GLenum value)
{
    switch (value)
    {
        case GL_HALF_FLOAT:
            return "GL_HALF_FLOAT";
        case GL_UNSIGNED_BYTE:
            return "GL_UNSIGNED_BYTE";
        case GL_BYTE:
            return "GL_BYTE";
        case GL_UNSIGNED_INT_24_8:
            return "GL_UNSIGNED_INT_24_8";
        case GL_UNSIGNED_SHORT:
            return "GL_UNSIGNED_SHORT";
        case GL_UNSIGNED_INT:
            return "GL_UNSIGNED_INT";
        case GL_FLOAT:
            return "GL_FLOAT";
        case GL_SHORT:
            return "GL_SHORT";
        case GL_INT:
            return "GL_INT";
        default:
            RNDR_HALT("Unsupported data type");
    }
    return "";
}

Rndr::String Rndr::FromOpenGLUsageToString(GLenum value)
{
    switch (value)
    {
        case GL_MAP_WRITE_BIT:
            return "GL_MAP_WRITE_BIT";
        case GL_DYNAMIC_STORAGE_BIT:
            return "GL_DYNAMIC_STORAGE_BIT";
        case GL_MAP_READ_BIT:
            return "GL_MAP_READ_BIT";
        default:
            RNDR_HALT("Unsupported usage");
    }
    return "";
}

#endif  // RNDR_OPENGL
