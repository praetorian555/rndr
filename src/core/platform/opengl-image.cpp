#include "rndr/core/platform/opengl-image.h"

#include <glad/glad.h>

#include "core/platform/opengl-helpers.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/platform/opengl-graphics-context.h"
#include "rndr/utility/cpu-tracer.h"
#include "rndr/core/bitmap.h"

Rndr::Image::Image(const GraphicsContext& graphics_context, const ImageDesc& desc, const ConstByteSpan& init_data) : m_desc(desc)
{
    RNDR_TRACE_SCOPED(Create Image);

    RNDR_UNUSED(graphics_context);

    // TODO: Add support for multi sample images
    constexpr bool k_is_multi_sample = false;
    const GLenum target = FromImageInfoToTarget(desc.type, k_is_multi_sample);
    glCreateTextures(target, 1, &m_native_texture);
    RNDR_ASSERT_OPENGL();
    const GLint min_filter = desc.use_mips ? FromMinAndMipFiltersToOpenGL(desc.sampler.min_filter, desc.sampler.mip_map_filter)
                                           : FromImageFilterToOpenGL(desc.sampler.min_filter);
    const GLint mag_filter = FromImageFilterToOpenGL(desc.sampler.mag_filter);
    glTextureParameteri(m_native_texture, GL_TEXTURE_MIN_FILTER, min_filter);
    glTextureParameteri(m_native_texture, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTextureParameterf(m_native_texture, GL_TEXTURE_MAX_ANISOTROPY, desc.sampler.max_anisotropy);
    RNDR_ASSERT_OPENGL();
    const GLint address_mode_u = FromImageAddressModeToOpenGL(desc.sampler.address_mode_u);
    const GLint address_mode_v = FromImageAddressModeToOpenGL(desc.sampler.address_mode_v);
    const GLint address_mode_w = FromImageAddressModeToOpenGL(desc.sampler.address_mode_w);
    glTextureParameteri(m_native_texture, GL_TEXTURE_WRAP_S, address_mode_u);
    glTextureParameteri(m_native_texture, GL_TEXTURE_WRAP_T, address_mode_v);
    glTextureParameteri(m_native_texture, GL_TEXTURE_WRAP_R, address_mode_w);
    RNDR_ASSERT_OPENGL();
    glTextureParameterfv(m_native_texture, GL_TEXTURE_BORDER_COLOR, desc.sampler.border_color.data);
    glTextureParameteri(m_native_texture, GL_TEXTURE_BASE_LEVEL, desc.sampler.base_mip_level);
    glTextureParameteri(m_native_texture, GL_TEXTURE_MAX_LEVEL, desc.sampler.max_mip_level);
    glTextureParameterf(m_native_texture, GL_TEXTURE_LOD_BIAS, desc.sampler.lod_bias);
    glTextureParameterf(m_native_texture, GL_TEXTURE_MIN_LOD, desc.sampler.min_lod);
    glTextureParameterf(m_native_texture, GL_TEXTURE_MAX_LOD, desc.sampler.max_lod);
    RNDR_ASSERT_OPENGL();
    // TODO(Marko): Left to handle GL_DEPTH_STENCIL_TEXTURE_MODE, GL_TEXTURE_COMPARE_FUNC, GL_TEXTURE_COMPARE_MODE

    const Vector2f size{static_cast<float>(desc.width), static_cast<float>(desc.height)};
    int mip_map_levels = 1;
    if (desc.use_mips)
    {
        mip_map_levels += static_cast<int>(Math::Floor(Math::Log2(Math::Max(size.x, size.y))));
    }
    if (desc.sampler.max_mip_level == 0 && desc.use_mips)
    {
        glTextureParameteri(m_native_texture, GL_TEXTURE_MAX_LEVEL, mip_map_levels - 1);
        RNDR_ASSERT_OPENGL();
    }
    if (desc.type == ImageType::Image2D)
    {
        const GLenum internal_format = FromPixelFormatToInternalFormat(desc.pixel_format);
        const GLenum format = FromPixelFormatToExternalFormat(desc.pixel_format);
        const GLenum data_type = FromPixelFormatToDataType(desc.pixel_format);
        glTextureStorage2D(m_native_texture, mip_map_levels, internal_format, desc.width, desc.height);
        RNDR_ASSERT_OPENGL();
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        RNDR_ASSERT_OPENGL();
        constexpr int32_t k_mip_level = 0;
        constexpr int32_t k_x_offset = 0;
        constexpr int32_t k_y_offset = 0;
        if (!init_data.empty())
        {
            glTextureSubImage2D(m_native_texture, k_mip_level, k_x_offset, k_y_offset, desc.width, desc.height, format, data_type,
                                init_data.data());
            RNDR_ASSERT_OPENGL();
        }
        if (desc.use_mips)
        {
            glGenerateTextureMipmap(m_native_texture);
            RNDR_ASSERT_OPENGL();
        }
    }
    else if (desc.type == ImageType::CubeMap)
    {
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        RNDR_ASSERT_OPENGL();
        const GLenum internal_format = FromPixelFormatToInternalFormat(desc.pixel_format);
        const GLenum format = FromPixelFormatToExternalFormat(desc.pixel_format);
        const GLenum data_type = FromPixelFormatToDataType(desc.pixel_format);
        glTextureStorage2D(m_native_texture, mip_map_levels, internal_format, desc.width, desc.height);
        RNDR_ASSERT_OPENGL();
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        RNDR_ASSERT_OPENGL();
        constexpr int k_face_count = 6;
        for (int i = 0; i < k_face_count; i++)
        {
            constexpr int32_t k_mip_level = 0;
            constexpr int32_t k_x_offset = 0;
            constexpr int32_t k_y_offset = 0;
            constexpr int32_t k_depth = 1;
            const int32_t z_offset = i;
            const uint8_t* data = init_data.data() + i * desc.width * desc.height * FromPixelFormatToPixelSize(desc.pixel_format);
            glTextureSubImage3D(m_native_texture, k_mip_level, k_x_offset, k_y_offset, z_offset, desc.width, desc.height, k_depth, format,
                                data_type, data);
            RNDR_ASSERT_OPENGL();
        }
        if (desc.use_mips)
        {
            glGenerateTextureMipmap(m_native_texture);
            RNDR_ASSERT_OPENGL();
        }
    }
    else
    {
        RNDR_HALT("Unsupported image type!");
    }

    if (m_desc.is_bindless)
    {
        m_bindless_handle = glGetTextureHandleARB(m_native_texture);
        RNDR_ASSERT_OPENGL();
        glMakeTextureHandleResidentARB(m_bindless_handle);
        RNDR_ASSERT_OPENGL();
    }
}

Rndr::Image::Image(const GraphicsContext& graphics_context, Bitmap& bitmap, bool use_mips, const SamplerDesc& sampler_desc)
    : Image(graphics_context,
            ImageDesc{.width = bitmap.GetWidth(),
                      .height = bitmap.GetHeight(),
                      .type = ImageType::Image2D,
                      .pixel_format = bitmap.GetPixelFormat(),
                      .use_mips = use_mips,
                      .sampler = sampler_desc},
            ByteSpan(bitmap.GetData(), bitmap.GetSize2D()))
{
}

Rndr::Image::~Image()
{
    Destroy();
}

Rndr::Image::Image(Image&& other) noexcept : m_desc(other.m_desc), m_native_texture(other.m_native_texture)
{
    other.m_native_texture = k_invalid_opengl_object;
}

Rndr::Image& Rndr::Image::operator=(Image&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_native_texture = other.m_native_texture;
        other.m_native_texture = k_invalid_opengl_object;
    }
    return *this;
}

void Rndr::Image::Destroy()
{
    if (m_native_texture != k_invalid_opengl_object)
    {
        glDeleteTextures(1, &m_native_texture);
        RNDR_ASSERT_OPENGL();
        m_native_texture = k_invalid_opengl_object;
    }
}

bool Rndr::Image::IsValid() const
{
    return m_native_texture != k_invalid_opengl_object;
}

const Rndr::ImageDesc& Rndr::Image::GetDesc() const
{
    return m_desc;
}

GLuint Rndr::Image::GetNativeTexture() const
{
    return m_native_texture;
}

uint64_t Rndr::Image::GetBindlessHandle() const
{
    return m_bindless_handle;
}
