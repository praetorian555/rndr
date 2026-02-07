#if RNDR_OPENGL

#include "glad/glad.h"

#include "opal/container/in-place-array.h"

#include "opengl-helpers.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/log.hpp"

constexpr Rndr::u64 k_max_shader_type = static_cast<Rndr::u64>(Rndr::ShaderType::EnumCount);
constexpr Opal::InPlaceArray<GLenum, k_max_shader_type> k_to_opengl_shader_type = {
    GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_COMPUTE_SHADER};

constexpr Rndr::u64 k_max_usage = static_cast<Rndr::u64>(Rndr::Usage::EnumCount);
constexpr Opal::InPlaceArray<GLenum, k_max_usage> k_to_opengl_usage = {GL_MAP_WRITE_BIT, GL_DYNAMIC_STORAGE_BIT, GL_MAP_READ_BIT};

constexpr Rndr::u64 k_max_buffer_type = static_cast<Rndr::u64>(Rndr::BufferType::EnumCount);
constexpr Opal::InPlaceArray<GLenum, k_max_buffer_type> k_to_opengl_buffer_type = {
    GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_UNIFORM_BUFFER, GL_SHADER_STORAGE_BUFFER, GL_DRAW_INDIRECT_BUFFER};

constexpr Rndr::u64 k_max_comparator = static_cast<Rndr::u64>(Rndr::Comparator::EnumCount);
constexpr Opal::InPlaceArray<GLenum, k_max_comparator> k_to_opengl_comparator = {GL_NEVER, GL_ALWAYS,   GL_LESS,   GL_GREATER,
                                                                                 GL_EQUAL, GL_NOTEQUAL, GL_LEQUAL, GL_GEQUAL};

constexpr Rndr::u64 k_max_stencil_op = static_cast<Rndr::u64>(Rndr::StencilOperation::EnumCount);
constexpr Opal::InPlaceArray<GLenum, k_max_stencil_op> k_to_opengl_stencil_op = {GL_KEEP,      GL_ZERO, GL_REPLACE,   GL_INCR,
                                                                                 GL_INCR_WRAP, GL_DECR, GL_DECR_WRAP, GL_INVERT};

constexpr Rndr::u64 k_max_blend_factor = static_cast<Rndr::u64>(Rndr::BlendFactor::EnumCount);
constexpr Opal::InPlaceArray<GLenum, k_max_blend_factor> k_to_opengl_blend_factor = {GL_ZERO,
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
constexpr Opal::InPlaceArray<GLenum, k_max_blend_op> k_to_opengl_blend_op = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT,
                                                                             GL_MIN, GL_MAX};

constexpr Rndr::u64 k_max_image_address_mode = static_cast<Rndr::u64>(Rndr::ImageAddressMode::EnumCount);
constexpr Opal::InPlaceArray<GLint, k_max_image_address_mode> k_to_opengl_image_address_mode = {
    GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_REPEAT, GL_MIRRORED_REPEAT, GL_MIRROR_CLAMP_TO_EDGE};

constexpr Rndr::u64 k_max_primitive_topology = static_cast<Rndr::u64>(Rndr::PrimitiveTopology::EnumCount);
constexpr Opal::InPlaceArray<GLenum, k_max_primitive_topology> k_to_opengl_primitive_topology = {GL_POINTS, GL_LINES, GL_LINE_STRIP,
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
    switch (format)
    {
        // R8 formats
        case PixelFormat::R8_UNORM:
            return GL_R8;
        case PixelFormat::R8_SNORM:
            return GL_R8_SNORM;
        case PixelFormat::R8_UINT:
            return GL_R8UI;
        case PixelFormat::R8_SINT:
            return GL_R8I;

        // RG8 formats
        case PixelFormat::R8G8_UNORM:
            return GL_RG8;
        case PixelFormat::R8G8_SNORM:
            return GL_RG8_SNORM;
        case PixelFormat::R8G8_UINT:
            return GL_RG8UI;
        case PixelFormat::R8G8_SINT:
            return GL_RG8I;

        // RGB8 formats
        case PixelFormat::R8G8B8_UNORM:
            return GL_RGB8;
        case PixelFormat::R8G8B8_SNORM:
            return GL_RGB8_SNORM;
        case PixelFormat::R8G8B8_UINT:
            return GL_RGB8UI;
        case PixelFormat::R8G8B8_SINT:
            return GL_RGB8I;
        case PixelFormat::R8G8B8_SRGB:
            return GL_SRGB8;

        // RGBA8 formats
        case PixelFormat::R8G8B8A8_UNORM:
            return GL_RGBA8;
        case PixelFormat::R8G8B8A8_SNORM:
            return GL_RGBA8_SNORM;
        case PixelFormat::R8G8B8A8_UINT:
            return GL_RGBA8UI;
        case PixelFormat::R8G8B8A8_SINT:
            return GL_RGBA8I;
        case PixelFormat::R8G8B8A8_SRGB:
            return GL_SRGB8_ALPHA8;

        // BGRA8 formats (use RGBA internal format with BGRA external format)
        case PixelFormat::B8G8R8A8_UNORM:
            return GL_RGBA8;
        case PixelFormat::B8G8R8A8_SRGB:
            return GL_SRGB8_ALPHA8;

        // A8B8G8R8 packed formats (same as RGBA8 in OpenGL)
        case PixelFormat::A8B8G8R8_UNORM_PACK32:
            return GL_RGBA8;
        case PixelFormat::A8B8G8R8_SNORM_PACK32:
            return GL_RGBA8_SNORM;
        case PixelFormat::A8B8G8R8_UINT_PACK32:
            return GL_RGBA8UI;
        case PixelFormat::A8B8G8R8_SINT_PACK32:
            return GL_RGBA8I;
        case PixelFormat::A8B8G8R8_SRGB_PACK32:
            return GL_SRGB8_ALPHA8;

        // A2B10G10R10 formats
        case PixelFormat::A2B10G10R10_UNORM_PACK32:
            return GL_RGB10_A2;
        case PixelFormat::A2B10G10R10_UINT_PACK32:
            return GL_RGB10_A2UI;

        // R16 formats
        case PixelFormat::R16_UNORM:
            return GL_R16;
        case PixelFormat::R16_SNORM:
            return GL_R16_SNORM;
        case PixelFormat::R16_UINT:
            return GL_R16UI;
        case PixelFormat::R16_SINT:
            return GL_R16I;
        case PixelFormat::R16_SFLOAT:
            return GL_R16F;

        // RG16 formats
        case PixelFormat::R16G16_UNORM:
            return GL_RG16;
        case PixelFormat::R16G16_SNORM:
            return GL_RG16_SNORM;
        case PixelFormat::R16G16_UINT:
            return GL_RG16UI;
        case PixelFormat::R16G16_SINT:
            return GL_RG16I;
        case PixelFormat::R16G16_SFLOAT:
            return GL_RG16F;

        // RGB16 formats
        case PixelFormat::R16G16B16_UNORM:
            return GL_RGB16;
        case PixelFormat::R16G16B16_SNORM:
            return GL_RGB16_SNORM;
        case PixelFormat::R16G16B16_UINT:
            return GL_RGB16UI;
        case PixelFormat::R16G16B16_SINT:
            return GL_RGB16I;
        case PixelFormat::R16G16B16_SFLOAT:
            return GL_RGB16F;

        // RGBA16 formats
        case PixelFormat::R16G16B16A16_UNORM:
            return GL_RGBA16;
        case PixelFormat::R16G16B16A16_SNORM:
            return GL_RGBA16_SNORM;
        case PixelFormat::R16G16B16A16_UINT:
            return GL_RGBA16UI;
        case PixelFormat::R16G16B16A16_SINT:
            return GL_RGBA16I;
        case PixelFormat::R16G16B16A16_SFLOAT:
            return GL_RGBA16F;

        // R32 formats
        case PixelFormat::R32_UINT:
            return GL_R32UI;
        case PixelFormat::R32_SINT:
            return GL_R32I;
        case PixelFormat::R32_SFLOAT:
            return GL_R32F;

        // RG32 formats
        case PixelFormat::R32G32_UINT:
            return GL_RG32UI;
        case PixelFormat::R32G32_SINT:
            return GL_RG32I;
        case PixelFormat::R32G32_SFLOAT:
            return GL_RG32F;

        // RGB32 formats
        case PixelFormat::R32G32B32_UINT:
            return GL_RGB32UI;
        case PixelFormat::R32G32B32_SINT:
            return GL_RGB32I;
        case PixelFormat::R32G32B32_SFLOAT:
            return GL_RGB32F;

        // RGBA32 formats
        case PixelFormat::R32G32B32A32_UINT:
            return GL_RGBA32UI;
        case PixelFormat::R32G32B32A32_SINT:
            return GL_RGBA32I;
        case PixelFormat::R32G32B32A32_SFLOAT:
            return GL_RGBA32F;

        // Special packed formats
        case PixelFormat::B10G11R11_UFLOAT_PACK32:
            return GL_R11F_G11F_B10F;
        case PixelFormat::E5B9G9R9_UFLOAT_PACK32:
            return GL_RGB9_E5;

        // Depth/stencil formats
        case PixelFormat::D16_UNORM:
            return GL_DEPTH_COMPONENT16;
        case PixelFormat::X8_D24_UNORM_PACK32:
            return GL_DEPTH_COMPONENT24;
        case PixelFormat::D32_SFLOAT:
            return GL_DEPTH_COMPONENT32F;
        case PixelFormat::S8_UINT:
            return GL_STENCIL_INDEX8;
        case PixelFormat::D24_UNORM_S8_UINT:
            return GL_DEPTH24_STENCIL8;
        case PixelFormat::D32_SFLOAT_S8_UINT:
            return GL_DEPTH32F_STENCIL8;

        // BC compressed formats (RGTC, core GL 3.0)
        case PixelFormat::BC4_UNORM_BLOCK:
            return GL_COMPRESSED_RED_RGTC1;
        case PixelFormat::BC4_SNORM_BLOCK:
            return GL_COMPRESSED_SIGNED_RED_RGTC1;
        case PixelFormat::BC5_UNORM_BLOCK:
            return GL_COMPRESSED_RG_RGTC2;
        case PixelFormat::BC5_SNORM_BLOCK:
            return GL_COMPRESSED_SIGNED_RG_RGTC2;

        // BC compressed formats (BPTC, core GL 4.2)
        case PixelFormat::BC6H_UFLOAT_BLOCK:
            return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
        case PixelFormat::BC6H_SFLOAT_BLOCK:
            return GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
        case PixelFormat::BC7_UNORM_BLOCK:
            return GL_COMPRESSED_RGBA_BPTC_UNORM;
        case PixelFormat::BC7_SRGB_BLOCK:
            return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;

        default:
            throw GraphicsAPIException(0, "Pixel format not supported by OpenGL!");
    }
}

GLenum Rndr::FromPixelFormatToExternalFormat(PixelFormat format)
{
    switch (format)
    {
        // Single-channel non-integer formats
        case PixelFormat::R8_UNORM:
        case PixelFormat::R8_SNORM:
        case PixelFormat::R16_UNORM:
        case PixelFormat::R16_SNORM:
        case PixelFormat::R16_SFLOAT:
        case PixelFormat::R32_SFLOAT:
            return GL_RED;

        // Single-channel integer formats
        case PixelFormat::R8_UINT:
        case PixelFormat::R8_SINT:
        case PixelFormat::R16_UINT:
        case PixelFormat::R16_SINT:
        case PixelFormat::R32_UINT:
        case PixelFormat::R32_SINT:
            return GL_RED_INTEGER;

        // Two-channel non-integer formats
        case PixelFormat::R8G8_UNORM:
        case PixelFormat::R8G8_SNORM:
        case PixelFormat::R16G16_UNORM:
        case PixelFormat::R16G16_SNORM:
        case PixelFormat::R16G16_SFLOAT:
        case PixelFormat::R32G32_SFLOAT:
            return GL_RG;

        // Two-channel integer formats
        case PixelFormat::R8G8_UINT:
        case PixelFormat::R8G8_SINT:
        case PixelFormat::R16G16_UINT:
        case PixelFormat::R16G16_SINT:
        case PixelFormat::R32G32_UINT:
        case PixelFormat::R32G32_SINT:
            return GL_RG_INTEGER;

        // Three-channel non-integer formats
        case PixelFormat::R8G8B8_UNORM:
        case PixelFormat::R8G8B8_SNORM:
        case PixelFormat::R8G8B8_SRGB:
        case PixelFormat::R16G16B16_UNORM:
        case PixelFormat::R16G16B16_SNORM:
        case PixelFormat::R16G16B16_SFLOAT:
        case PixelFormat::R32G32B32_SFLOAT:
        case PixelFormat::B10G11R11_UFLOAT_PACK32:
        case PixelFormat::E5B9G9R9_UFLOAT_PACK32:
            return GL_RGB;

        // Three-channel integer formats
        case PixelFormat::R8G8B8_UINT:
        case PixelFormat::R8G8B8_SINT:
        case PixelFormat::R16G16B16_UINT:
        case PixelFormat::R16G16B16_SINT:
        case PixelFormat::R32G32B32_UINT:
        case PixelFormat::R32G32B32_SINT:
            return GL_RGB_INTEGER;

        // Four-channel non-integer formats
        case PixelFormat::R8G8B8A8_UNORM:
        case PixelFormat::R8G8B8A8_SNORM:
        case PixelFormat::R8G8B8A8_SRGB:
        case PixelFormat::A8B8G8R8_UNORM_PACK32:
        case PixelFormat::A8B8G8R8_SNORM_PACK32:
        case PixelFormat::A8B8G8R8_SRGB_PACK32:
        case PixelFormat::A2B10G10R10_UNORM_PACK32:
        case PixelFormat::R16G16B16A16_UNORM:
        case PixelFormat::R16G16B16A16_SNORM:
        case PixelFormat::R16G16B16A16_SFLOAT:
        case PixelFormat::R32G32B32A32_SFLOAT:
            return GL_RGBA;

        // Four-channel integer formats
        case PixelFormat::R8G8B8A8_UINT:
        case PixelFormat::R8G8B8A8_SINT:
        case PixelFormat::A8B8G8R8_UINT_PACK32:
        case PixelFormat::A8B8G8R8_SINT_PACK32:
        case PixelFormat::A2B10G10R10_UINT_PACK32:
        case PixelFormat::R16G16B16A16_UINT:
        case PixelFormat::R16G16B16A16_SINT:
        case PixelFormat::R32G32B32A32_UINT:
        case PixelFormat::R32G32B32A32_SINT:
            return GL_RGBA_INTEGER;

        // BGRA formats
        case PixelFormat::B8G8R8A8_UNORM:
        case PixelFormat::B8G8R8A8_SRGB:
            return GL_BGRA;

        // Depth-only formats
        case PixelFormat::D16_UNORM:
        case PixelFormat::X8_D24_UNORM_PACK32:
        case PixelFormat::D32_SFLOAT:
            return GL_DEPTH_COMPONENT;

        // Stencil-only formats
        case PixelFormat::S8_UINT:
            return GL_STENCIL_INDEX;

        // Combined depth-stencil formats
        case PixelFormat::D24_UNORM_S8_UINT:
        case PixelFormat::D32_SFLOAT_S8_UINT:
            return GL_DEPTH_STENCIL;

        default:
            throw GraphicsAPIException(0, "Pixel format not supported by OpenGL!");
    }
}

GLenum Rndr::FromPixelFormatToDataType(PixelFormat format)
{
    switch (format)
    {
        // 8-bit unsigned
        case PixelFormat::R8_UNORM:
        case PixelFormat::R8_UINT:
        case PixelFormat::R8G8_UNORM:
        case PixelFormat::R8G8_UINT:
        case PixelFormat::R8G8B8_UNORM:
        case PixelFormat::R8G8B8_UINT:
        case PixelFormat::R8G8B8_SRGB:
        case PixelFormat::R8G8B8A8_UNORM:
        case PixelFormat::R8G8B8A8_UINT:
        case PixelFormat::R8G8B8A8_SRGB:
        case PixelFormat::B8G8R8A8_UNORM:
        case PixelFormat::B8G8R8A8_SRGB:
        case PixelFormat::A8B8G8R8_UNORM_PACK32:
        case PixelFormat::A8B8G8R8_UINT_PACK32:
        case PixelFormat::A8B8G8R8_SRGB_PACK32:
        case PixelFormat::S8_UINT:
            return GL_UNSIGNED_BYTE;

        // 8-bit signed
        case PixelFormat::R8_SNORM:
        case PixelFormat::R8_SINT:
        case PixelFormat::R8G8_SNORM:
        case PixelFormat::R8G8_SINT:
        case PixelFormat::R8G8B8_SNORM:
        case PixelFormat::R8G8B8_SINT:
        case PixelFormat::R8G8B8A8_SNORM:
        case PixelFormat::R8G8B8A8_SINT:
        case PixelFormat::A8B8G8R8_SNORM_PACK32:
        case PixelFormat::A8B8G8R8_SINT_PACK32:
            return GL_BYTE;

        // 16-bit unsigned
        case PixelFormat::R16_UNORM:
        case PixelFormat::R16_UINT:
        case PixelFormat::R16G16_UNORM:
        case PixelFormat::R16G16_UINT:
        case PixelFormat::R16G16B16_UNORM:
        case PixelFormat::R16G16B16_UINT:
        case PixelFormat::R16G16B16A16_UNORM:
        case PixelFormat::R16G16B16A16_UINT:
        case PixelFormat::D16_UNORM:
            return GL_UNSIGNED_SHORT;

        // 16-bit signed
        case PixelFormat::R16_SNORM:
        case PixelFormat::R16_SINT:
        case PixelFormat::R16G16_SNORM:
        case PixelFormat::R16G16_SINT:
        case PixelFormat::R16G16B16_SNORM:
        case PixelFormat::R16G16B16_SINT:
        case PixelFormat::R16G16B16A16_SNORM:
        case PixelFormat::R16G16B16A16_SINT:
            return GL_SHORT;

        // 16-bit float
        case PixelFormat::R16_SFLOAT:
        case PixelFormat::R16G16_SFLOAT:
        case PixelFormat::R16G16B16_SFLOAT:
        case PixelFormat::R16G16B16A16_SFLOAT:
            return GL_HALF_FLOAT;

        // 32-bit unsigned
        case PixelFormat::R32_UINT:
        case PixelFormat::R32G32_UINT:
        case PixelFormat::R32G32B32_UINT:
        case PixelFormat::R32G32B32A32_UINT:
        case PixelFormat::X8_D24_UNORM_PACK32:
            return GL_UNSIGNED_INT;

        // 32-bit signed
        case PixelFormat::R32_SINT:
        case PixelFormat::R32G32_SINT:
        case PixelFormat::R32G32B32_SINT:
        case PixelFormat::R32G32B32A32_SINT:
            return GL_INT;

        // 32-bit float
        case PixelFormat::R32_SFLOAT:
        case PixelFormat::R32G32_SFLOAT:
        case PixelFormat::R32G32B32_SFLOAT:
        case PixelFormat::R32G32B32A32_SFLOAT:
        case PixelFormat::D32_SFLOAT:
            return GL_FLOAT;

        // Special packed types
        case PixelFormat::A2B10G10R10_UNORM_PACK32:
        case PixelFormat::A2B10G10R10_UINT_PACK32:
            return GL_UNSIGNED_INT_2_10_10_10_REV;
        case PixelFormat::B10G11R11_UFLOAT_PACK32:
            return GL_UNSIGNED_INT_10F_11F_11F_REV;
        case PixelFormat::E5B9G9R9_UFLOAT_PACK32:
            return GL_UNSIGNED_INT_5_9_9_9_REV;

        // Depth-stencil packed types
        case PixelFormat::D24_UNORM_S8_UINT:
            return GL_UNSIGNED_INT_24_8;
        case PixelFormat::D32_SFLOAT_S8_UINT:
            return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;

        default:
            throw GraphicsAPIException(0, "Pixel format not supported by OpenGL!");
    }
}

GLenum Rndr::FromPixelFormatToShouldNormalizeData(PixelFormat format)
{
    switch (format)
    {
        // UNORM formats (normalized unsigned)
        case PixelFormat::R8_UNORM:
        case PixelFormat::R8G8_UNORM:
        case PixelFormat::R8G8B8_UNORM:
        case PixelFormat::R8G8B8A8_UNORM:
        case PixelFormat::B8G8R8A8_UNORM:
        case PixelFormat::A8B8G8R8_UNORM_PACK32:
        case PixelFormat::A2B10G10R10_UNORM_PACK32:
        case PixelFormat::R16_UNORM:
        case PixelFormat::R16G16_UNORM:
        case PixelFormat::R16G16B16_UNORM:
        case PixelFormat::R16G16B16A16_UNORM:
        // SNORM formats (normalized signed)
        case PixelFormat::R8_SNORM:
        case PixelFormat::R8G8_SNORM:
        case PixelFormat::R8G8B8_SNORM:
        case PixelFormat::R8G8B8A8_SNORM:
        case PixelFormat::A8B8G8R8_SNORM_PACK32:
        case PixelFormat::R16_SNORM:
        case PixelFormat::R16G16_SNORM:
        case PixelFormat::R16G16B16_SNORM:
        case PixelFormat::R16G16B16A16_SNORM:
        // SRGB formats (normalized)
        case PixelFormat::R8G8B8_SRGB:
        case PixelFormat::R8G8B8A8_SRGB:
        case PixelFormat::B8G8R8A8_SRGB:
        case PixelFormat::A8B8G8R8_SRGB_PACK32:
            return GL_TRUE;

        // UINT, SINT, SFLOAT formats (not normalized)
        case PixelFormat::R8_UINT:
        case PixelFormat::R8_SINT:
        case PixelFormat::R8G8_UINT:
        case PixelFormat::R8G8_SINT:
        case PixelFormat::R8G8B8_UINT:
        case PixelFormat::R8G8B8_SINT:
        case PixelFormat::R8G8B8A8_UINT:
        case PixelFormat::R8G8B8A8_SINT:
        case PixelFormat::A8B8G8R8_UINT_PACK32:
        case PixelFormat::A8B8G8R8_SINT_PACK32:
        case PixelFormat::A2B10G10R10_UINT_PACK32:
        case PixelFormat::R16_UINT:
        case PixelFormat::R16_SINT:
        case PixelFormat::R16_SFLOAT:
        case PixelFormat::R16G16_UINT:
        case PixelFormat::R16G16_SINT:
        case PixelFormat::R16G16_SFLOAT:
        case PixelFormat::R16G16B16_UINT:
        case PixelFormat::R16G16B16_SINT:
        case PixelFormat::R16G16B16_SFLOAT:
        case PixelFormat::R16G16B16A16_UINT:
        case PixelFormat::R16G16B16A16_SINT:
        case PixelFormat::R16G16B16A16_SFLOAT:
        case PixelFormat::R32_UINT:
        case PixelFormat::R32_SINT:
        case PixelFormat::R32_SFLOAT:
        case PixelFormat::R32G32_UINT:
        case PixelFormat::R32G32_SINT:
        case PixelFormat::R32G32_SFLOAT:
        case PixelFormat::R32G32B32_UINT:
        case PixelFormat::R32G32B32_SINT:
        case PixelFormat::R32G32B32_SFLOAT:
        case PixelFormat::R32G32B32A32_UINT:
        case PixelFormat::R32G32B32A32_SINT:
        case PixelFormat::R32G32B32A32_SFLOAT:
        case PixelFormat::B10G11R11_UFLOAT_PACK32:
        case PixelFormat::E5B9G9R9_UFLOAT_PACK32:
        // Depth/stencil formats
        case PixelFormat::D16_UNORM:
        case PixelFormat::X8_D24_UNORM_PACK32:
        case PixelFormat::D32_SFLOAT:
        case PixelFormat::S8_UINT:
        case PixelFormat::D24_UNORM_S8_UINT:
        case PixelFormat::D32_SFLOAT_S8_UINT:
        // Compressed formats
        case PixelFormat::BC4_UNORM_BLOCK:
        case PixelFormat::BC4_SNORM_BLOCK:
        case PixelFormat::BC5_UNORM_BLOCK:
        case PixelFormat::BC5_SNORM_BLOCK:
        case PixelFormat::BC6H_UFLOAT_BLOCK:
        case PixelFormat::BC6H_SFLOAT_BLOCK:
        case PixelFormat::BC7_UNORM_BLOCK:
        case PixelFormat::BC7_SRGB_BLOCK:
            return GL_FALSE;

        default:
            throw GraphicsAPIException(0, "Pixel format not supported by OpenGL!");
    }
}

bool Rndr::IsPixelFormatInteger(PixelFormat format)
{
    switch (format)
    {
        case PixelFormat::R8_UINT:
        case PixelFormat::R8_SINT:
        case PixelFormat::R8G8_UINT:
        case PixelFormat::R8G8_SINT:
        case PixelFormat::R8G8B8_UINT:
        case PixelFormat::R8G8B8_SINT:
        case PixelFormat::R8G8B8A8_UINT:
        case PixelFormat::R8G8B8A8_SINT:
        case PixelFormat::A8B8G8R8_UINT_PACK32:
        case PixelFormat::A8B8G8R8_SINT_PACK32:
        case PixelFormat::A2B10G10R10_UINT_PACK32:
        case PixelFormat::R16_UINT:
        case PixelFormat::R16_SINT:
        case PixelFormat::R16G16_UINT:
        case PixelFormat::R16G16_SINT:
        case PixelFormat::R16G16B16_UINT:
        case PixelFormat::R16G16B16_SINT:
        case PixelFormat::R16G16B16A16_UINT:
        case PixelFormat::R16G16B16A16_SINT:
        case PixelFormat::R32_UINT:
        case PixelFormat::R32_SINT:
        case PixelFormat::R32G32_UINT:
        case PixelFormat::R32G32_SINT:
        case PixelFormat::R32G32B32_UINT:
        case PixelFormat::R32G32B32_SINT:
        case PixelFormat::R32G32B32A32_UINT:
        case PixelFormat::R32G32B32A32_SINT:
            return true;

        default:
            return false;
    }
}

int32_t Rndr::FromPixelFormatToPixelSize(PixelFormat format)
{
    return static_cast<int32_t>(GetPixelSize(format));
}

GLint Rndr::FromPixelFormatToComponentCount(PixelFormat format)
{
    return static_cast<GLint>(GetComponentCount(format));
}

bool Rndr::IsComponentLowPrecision(PixelFormat format)
{
    return IsLowPrecisionFormat(format);
}

bool Rndr::IsComponentHighPrecision(PixelFormat format)
{
    return IsHighPrecisionFormat(format);
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

GLint Rndr::FromImageAccessToOpenGL(Rndr::TextureAccess access)
{
    switch (access)
    {
        case Rndr::TextureAccess::Read:
            return GL_READ_ONLY;
        case Rndr::TextureAccess::Write:
            return GL_WRITE_ONLY;
        case Rndr::TextureAccess::ReadWrite:
            return GL_READ_WRITE;
        default:
            RNDR_HALT("Unsupported image access");
    }
#if RNDR_DEBUG
    return GL_INVALID_ENUM;
#endif  // RNDR_DEBUG
}

Opal::StringUtf8 Rndr::FromBufferTypeToString(Rndr::BufferType type)
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
        case BufferType::DrawCommands:
            return "DrawCommands";
        default:
            RNDR_HALT("Invalid buffer type");
    }
#if RNDR_DEBUG
    return "";
#endif  // RNDR_DEBUG
}

Opal::StringUtf8 Rndr::FromOpenGLDataTypeToString(GLenum value)
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
#if RNDR_DEBUG
    return "";
#endif  // RNDR_DEBUG
}

Opal::StringUtf8 Rndr::FromOpenGLUsageToString(GLenum value)
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
#if RNDR_DEBUG
    return "";
#endif  // RNDR_DEBUG
}

#endif  // RNDR_OPENGL
