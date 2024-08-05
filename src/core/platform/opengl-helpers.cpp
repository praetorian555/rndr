#if RNDR_OPENGL

#include <glad/glad.h>

#include "opal/container/stack-array.h"

#include "core/platform/opengl-helpers.h"
#include "rndr/core/graphics-types.h"

constexpr Rndr::u64 k_max_shader_type = static_cast<Rndr::u64>(Rndr::ShaderType::EnumCount);
constexpr Opal::StackArray<GLenum, k_max_shader_type> k_to_opengl_shader_type = {
    GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_COMPUTE_SHADER};

constexpr Rndr::u64 k_max_usage = static_cast<Rndr::u64>(Rndr::Usage::EnumCount);
constexpr Opal::StackArray<GLenum, k_max_usage> k_to_opengl_usage = {GL_MAP_WRITE_BIT, GL_DYNAMIC_STORAGE_BIT, GL_MAP_READ_BIT};

constexpr Rndr::u64 k_max_buffer_type = static_cast<Rndr::u64>(Rndr::BufferType::EnumCount);
constexpr Opal::StackArray<GLenum, k_max_buffer_type> k_to_opengl_buffer_type = {GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER,
                                                                                 GL_UNIFORM_BUFFER, GL_SHADER_STORAGE_BUFFER};

constexpr Rndr::u64 k_max_comparator = static_cast<Rndr::u64>(Rndr::Comparator::EnumCount);
constexpr Opal::StackArray<GLenum, k_max_comparator> k_to_opengl_comparator = {GL_NEVER, GL_ALWAYS,   GL_LESS,   GL_GREATER,
                                                                               GL_EQUAL, GL_NOTEQUAL, GL_LEQUAL, GL_GEQUAL};

constexpr Rndr::u64 k_max_stencil_op = static_cast<Rndr::u64>(Rndr::StencilOperation::EnumCount);
constexpr Opal::StackArray<GLenum, k_max_stencil_op> k_to_opengl_stencil_op = {GL_KEEP,      GL_ZERO, GL_REPLACE,   GL_INCR,
                                                                               GL_INCR_WRAP, GL_DECR, GL_DECR_WRAP, GL_INVERT};

constexpr Rndr::u64 k_max_blend_factor = static_cast<Rndr::u64>(Rndr::BlendFactor::EnumCount);
constexpr Opal::StackArray<GLenum, k_max_blend_factor> k_to_opengl_blend_factor = {GL_ZERO,
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

constexpr Rndr::u64 k_max_blend_op = static_cast<Rndr::u64>(Rndr::BlendOperation::EnumCount);
constexpr Opal::StackArray<GLenum, k_max_blend_op> k_to_opengl_blend_op = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN,
                                                                           GL_MAX};

constexpr Rndr::u64 k_max_image_address_mode = static_cast<Rndr::u64>(Rndr::ImageAddressMode::EnumCount);
constexpr Opal::StackArray<GLint, k_max_image_address_mode> k_to_opengl_image_address_mode = {
    GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_REPEAT, GL_MIRRORED_REPEAT, GL_MIRROR_CLAMP_TO_EDGE};

constexpr Rndr::u64 k_max_pixel_format = static_cast<Rndr::u64>(Rndr::PixelFormat::EnumCount);
constexpr Opal::StackArray<GLenum, k_max_pixel_format> k_to_opengl_internal_pixel_format = {GL_RGBA8,
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
                                                                                            GL_R16F,

                                                                                            GL_RG16F};
constexpr Opal::StackArray<GLenum, k_max_pixel_format> k_to_opengl_external_pixel_format = {GL_RGBA,
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
                                                                                            GL_RED,

                                                                                            GL_RG};
constexpr Opal::StackArray<GLenum, k_max_pixel_format> k_to_opengl_pixel_type = {GL_UNSIGNED_BYTE,
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
                                                                                 GL_HALF_FLOAT,

                                                                                 GL_HALF_FLOAT};
constexpr Opal::StackArray<int32_t, k_max_pixel_format> k_to_pixel_size = {4, 4, 4, 4, 4,  4,  4,  4,  3,  3,  3, 3, 3, 2, 2, 2, 2, 2, 1,
                                                                           1, 1, 1, 1, 16, 16, 16, 12, 12, 12, 8, 8, 8, 4, 4, 4, 4, 2, 4};
constexpr Opal::StackArray<GLint, k_max_pixel_format> k_to_component_count = {4, 4, 4, 4, 4, 4, 4, 2, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1,
                                                                              1, 1, 1, 1, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 2};
constexpr Opal::StackArray<GLenum, k_max_pixel_format> k_to_should_normalize_data = {
    GL_TRUE,  GL_TRUE,  GL_FALSE, GL_TRUE,  GL_FALSE, GL_TRUE,  GL_TRUE,  GL_FALSE, GL_TRUE,  GL_TRUE,  GL_FALSE, GL_TRUE,  GL_FALSE,
    GL_TRUE,  GL_TRUE,  GL_FALSE, GL_TRUE,  GL_FALSE, GL_TRUE,  GL_TRUE,  GL_FALSE, GL_TRUE,  GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE,
    GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE};

constexpr Opal::StackArray<bool, k_max_pixel_format> k_is_integer = {
    false, false, true,  false, true,  false, false, false, false, false, true,  false, true, false, false, true, false, true, false,
    false, true,  false, true,  false, true,  true,  false, true,  true,  false, true,  true, false, true,  true, false, false, false};

constexpr Rndr::u64 k_max_primitive_topology = static_cast<Rndr::u64>(Rndr::PrimitiveTopology::EnumCount);
constexpr Opal::StackArray<GLenum, k_max_primitive_topology> k_to_opengl_primitive_topology = {GL_POINTS, GL_LINES, GL_LINE_STRIP,
                                                                                               GL_TRIANGLES, GL_TRIANGLE_STRIP};

GLenum Rndr::FromShaderTypeToOpenGL(ShaderType type)
{
    return k_to_opengl_shader_type[static_cast<Rndr::u64>(type)];
}

GLenum Rndr::FromUsageToOpenGL(Usage usage)
{
    return k_to_opengl_usage[static_cast<Rndr::u64>(usage)];
}

GLenum Rndr::FromBufferTypeToOpenGL(BufferType type)
{
    return k_to_opengl_buffer_type[static_cast<Rndr::u64>(type)];
}

GLenum Rndr::FromComparatorToOpenGL(Comparator comparator)
{
    return k_to_opengl_comparator[static_cast<Rndr::u64>(comparator)];
}

GLenum Rndr::FromStencilOpToOpenGL(StencilOperation op)
{
    return k_to_opengl_stencil_op[static_cast<Rndr::u64>(op)];
}

GLenum Rndr::FromBlendFactorToOpenGL(BlendFactor factor)
{
    return k_to_opengl_blend_factor[static_cast<Rndr::u64>(factor)];
}

GLenum Rndr::FromBlendOperationToOpenGL(BlendOperation op)
{
    return k_to_opengl_blend_op[static_cast<Rndr::u64>(op)];
}

GLenum Rndr::FromImageInfoToTarget(TextureType image_type, bool is_multi_sample)
{
    GLenum target = GL_TEXTURE_2D;
    switch (image_type)
    {
        case TextureType::Texture2D:
            target = is_multi_sample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
            break;
        case TextureType::Texture2DArray:
            target = is_multi_sample ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
            break;
        case TextureType::CubeMap:
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
    return k_to_opengl_image_address_mode[static_cast<Rndr::u64>(address_mode)];
}

GLenum Rndr::FromPixelFormatToInternalFormat(PixelFormat format)
{
    return k_to_opengl_internal_pixel_format[static_cast<Rndr::u64>(format)];
}

GLenum Rndr::FromPixelFormatToExternalFormat(PixelFormat format)
{
    return k_to_opengl_external_pixel_format[static_cast<Rndr::u64>(format)];
}

GLenum Rndr::FromPixelFormatToDataType(PixelFormat format)
{
    return k_to_opengl_pixel_type[static_cast<Rndr::u64>(format)];
}

int32_t Rndr::FromPixelFormatToPixelSize(PixelFormat format)
{
    return k_to_pixel_size[static_cast<Rndr::u64>(format)];
}

GLint Rndr::FromPixelFormatToComponentCount(PixelFormat format)
{
    return k_to_component_count[static_cast<Rndr::u64>(format)];
}

GLenum Rndr::FromPixelFormatToShouldNormalizeData(Rndr::PixelFormat format)
{
    return k_to_should_normalize_data[static_cast<Rndr::u64>(format)];
}

bool Rndr::IsPixelFormatInteger(Rndr::PixelFormat format)
{
    return k_is_integer[static_cast<Rndr::u64>(format)];
}

GLenum Rndr::FromPrimitiveTopologyToOpenGL(PrimitiveTopology topology)
{
    return k_to_opengl_primitive_topology[static_cast<Rndr::u64>(topology)];
}

GLenum Rndr::FromIndexSizeToOpenGL(int64_t index_size)
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
    }
#if RNDR_DEBUG
    return GL_INVALID_ENUM;
#endif  // RNDR_DEBUG
}

GLint Rndr::FromImageAccessToOpenGL(Rndr::ImageAccess access)
{
    switch (access)
    {
        case Rndr::ImageAccess::Read:
            return GL_READ_ONLY;
        case Rndr::ImageAccess::Write:
            return GL_WRITE_ONLY;
        case Rndr::ImageAccess::ReadWrite:
            return GL_READ_WRITE;
        default:
            RNDR_HALT("Unsupported image access");
    }
#if RNDR_DEBUG
    return GL_INVALID_ENUM;
#endif  // RNDR_DEBUG
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

Opal::StringUtf8 Rndr::FromBufferTypeToString(Rndr::BufferType type)
{
    switch (type)
    {
        case BufferType::Vertex:
            return u8"Vertex";
        case BufferType::Index:
            return u8"Index";
        case BufferType::Constant:
            return u8"Constant";
        case BufferType::ShaderStorage:
            return u8"Storage";
        default:
            RNDR_HALT("Invalid buffer type");
    }
#if RNDR_DEBUG
    return u8"";
#endif  // RNDR_DEBUG
}

Opal::StringUtf8 Rndr::FromOpenGLDataTypeToString(GLenum value)
{
    switch (value)
    {
        case GL_HALF_FLOAT:
            return u8"GL_HALF_FLOAT";
        case GL_UNSIGNED_BYTE:
            return u8"GL_UNSIGNED_BYTE";
        case GL_BYTE:
            return u8"GL_BYTE";
        case GL_UNSIGNED_INT_24_8:
            return u8"GL_UNSIGNED_INT_24_8";
        case GL_UNSIGNED_SHORT:
            return u8"GL_UNSIGNED_SHORT";
        case GL_UNSIGNED_INT:
            return u8"GL_UNSIGNED_INT";
        case GL_FLOAT:
            return u8"GL_FLOAT";
        case GL_SHORT:
            return u8"GL_SHORT";
        case GL_INT:
            return u8"GL_INT";
        default:
            RNDR_HALT("Unsupported data type");
    }
#if RNDR_DEBUG
    return u8"";
#endif  // RNDR_DEBUG
}

Opal::StringUtf8 Rndr::FromOpenGLUsageToString(GLenum value)
{
    switch (value)
    {
        case GL_MAP_WRITE_BIT:
            return u8"GL_MAP_WRITE_BIT";
        case GL_DYNAMIC_STORAGE_BIT:
            return u8"GL_DYNAMIC_STORAGE_BIT";
        case GL_MAP_READ_BIT:
            return u8"GL_MAP_READ_BIT";
        default:
            RNDR_HALT("Unsupported usage");
    }
#if RNDR_DEBUG
    return u8"";
#endif  // RNDR_DEBUG
}

#endif  // RNDR_OPENGL
