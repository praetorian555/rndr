#include "rndr/platform/opengl-texture.h"

#include "glad/glad.h"

#include "opengl-helpers.h"
#include "rndr/bitmap.h"
#include "rndr/log.h"
#include "rndr/platform/opengl-graphics-context.h"
#include "rndr/trace.h"

Rndr::Texture::Texture(const GraphicsContext& graphics_context, const TextureDesc& texture_desc, const SamplerDesc& sampler_desc,
                       const Opal::Span<const u8>& init_data)
{
    const ErrorCode err = Initialize(graphics_context, texture_desc, sampler_desc, init_data);
    if (err != ErrorCode::Success)
    {
        Destroy();
    }
}

namespace
{
Rndr::ErrorCode CheckTextureDesc(const Rndr::GraphicsContext& graphics_context, const Rndr::TextureDesc& texture_desc)
{

    if (texture_desc.width <= 0 || texture_desc.height <= 0)
    {
        RNDR_LOG_ERROR("Texture width and height must be greater than 0!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (texture_desc.type >= Rndr::TextureType::EnumCount)
    {
        RNDR_LOG_ERROR("Texture type is invalid!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (texture_desc.type == Rndr::TextureType::Texture2DArray && texture_desc.array_size <= 0)
    {
        RNDR_LOG_ERROR("Texture2DArray must have array size greater than 0!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (texture_desc.pixel_format >= Rndr::PixelFormat::EnumCount)
    {
        RNDR_LOG_ERROR("Texture pixel format is invalid!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (!graphics_context.GetDesc().enable_bindless_textures && texture_desc.is_bindless)
    {
        RNDR_LOG_ERROR("Bindless textures are disabled for this graphics context!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    Rndr::i32 max_sample_count = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &max_sample_count);
    if (texture_desc.sample_count < 1 || texture_desc.sample_count > max_sample_count)
    {
        RNDR_LOG_ERROR("Texture sample count must be between [1, %d]!", max_sample_count);
        return Rndr::ErrorCode::InvalidArgument;
    }
    return Rndr::ErrorCode::Success;
}

Rndr::ErrorCode CheckSamplerDesc(const Rndr::GraphicsContext& graphics_context, const Rndr::SamplerDesc& sampler_desc)
{
    if (sampler_desc.min_filter >= Rndr::ImageFilter::EnumCount)
    {
        RNDR_LOG_ERROR("Sampler min filter is invalid!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (sampler_desc.mag_filter >= Rndr::ImageFilter::EnumCount)
    {
        RNDR_LOG_ERROR("Sampler mag filter is invalid!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (sampler_desc.mip_map_filter >= Rndr::ImageFilter::EnumCount)
    {
        RNDR_LOG_ERROR("Sampler mip map filter is invalid!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    float max_texture_anisotropy = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_texture_anisotropy);
    if (sampler_desc.max_anisotropy < 1.0f || sampler_desc.max_anisotropy > max_texture_anisotropy)
    {
        RNDR_LOG_ERROR("Sampler max anisotropy must be between [1.0f, %f]!", max_texture_anisotropy);
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (sampler_desc.address_mode_u >= Rndr::ImageAddressMode::EnumCount)
    {
        RNDR_LOG_ERROR("Sampler address mode u is invalid!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (sampler_desc.address_mode_v >= Rndr::ImageAddressMode::EnumCount)
    {
        RNDR_LOG_ERROR("Sampler address mode v is invalid!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (sampler_desc.address_mode_w >= Rndr::ImageAddressMode::EnumCount)
    {
        RNDR_LOG_ERROR("Sampler address mode w is invalid!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (graphics_context.GetDesc().enable_bindless_textures)
    {
        constexpr Opal::StackArray<Rndr::Vector4f, 4> k_allowed_border_colors = {
            Rndr::Vector4f{0.0f, 0.0f, 0.0f, 0.0f}, Rndr::Vector4f{1.0f, 1.0f, 1.0f, 1.0f}, Rndr::Vector4f{0.0f, 0.0f, 0.0f, 1.0f},
            Rndr::Vector4f{1.0f, 1.0f, 1.0f, 0.0f}};
        bool found_border_color = false;
        for (const Rndr::Vector4f& border_color : k_allowed_border_colors)
        {
            if (sampler_desc.border_color == border_color)
            {
                found_border_color = true;
                break;
            }
        }
        if (!found_border_color)
        {
            RNDR_LOG_ERROR("Sampler border color is invalid for use with bindless textures!");
            return Rndr::ErrorCode::InvalidArgument;
        }
    }
    if (sampler_desc.base_mip_level < 0)
    {
        RNDR_LOG_ERROR("Sampler base mip level must be greater than or equal to 0!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (sampler_desc.max_mip_level < 0)
    {
        RNDR_LOG_ERROR("Sampler max mip level must be greater than or equal to 0!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    return Rndr::ErrorCode::Success;
}
}  // namespace

Rndr::ErrorCode Rndr::Texture::Initialize(const GraphicsContext& graphics_context, const TextureDesc& texture_desc,
                                          const SamplerDesc& sampler_desc, const Opal::Span<const u8>& init_data)
{
    RNDR_CPU_EVENT_SCOPED("Create Texture");

    ErrorCode err = CheckTextureDesc(graphics_context, texture_desc);
    if (err != ErrorCode::Success)
    {
        return err;
    }
    err = CheckSamplerDesc(graphics_context, sampler_desc);
    if (err != ErrorCode::Success)
    {
        return err;
    }
    m_texture_desc = texture_desc;
    m_sampler_desc = sampler_desc;

    // Figure out the number of mips to be used.
    const Vector2f size{static_cast<f32>(m_texture_desc.width), static_cast<f32>(m_texture_desc.height)};
    m_max_mip_levels = 1;
    if (m_texture_desc.use_mips)
    {
        m_max_mip_levels += static_cast<i32>(Math::Floor(Math::Log2(Math::Max(size.x, size.y))));
    }

    const GLenum target = FromImageInfoToTarget(m_texture_desc.type, m_texture_desc.sample_count > 1);
    glCreateTextures(target, 1, &m_native_texture);
    if (m_native_texture == k_invalid_opengl_object)
    {
        RNDR_LOG_ERROR("Failed to create texture!");
        return ErrorCode::OutOfMemory;
    }

    // Set sampler parameters
    if (m_texture_desc.sample_count == 1)
    {
        const GLint min_filter = m_texture_desc.use_mips
                                     ? FromMinAndMipFiltersToOpenGL(m_sampler_desc.min_filter, m_sampler_desc.mip_map_filter)
                                     : FromImageFilterToOpenGL(m_sampler_desc.min_filter);
        glTextureParameteri(m_native_texture, GL_TEXTURE_MIN_FILTER, min_filter);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_MIN_FILTER!", Destroy());
        const GLint mag_filter = FromImageFilterToOpenGL(m_sampler_desc.mag_filter);
        glTextureParameteri(m_native_texture, GL_TEXTURE_MAG_FILTER, mag_filter);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_MAG_FILTER!", Destroy());
        glTextureParameterf(m_native_texture, GL_TEXTURE_MAX_ANISOTROPY, m_sampler_desc.max_anisotropy);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_MAX_ANISOTROPY!", Destroy());
        const GLint address_mode_u = FromImageAddressModeToOpenGL(m_sampler_desc.address_mode_u);
        const GLint address_mode_v = FromImageAddressModeToOpenGL(m_sampler_desc.address_mode_v);
        const GLint address_mode_w = FromImageAddressModeToOpenGL(m_sampler_desc.address_mode_w);
        glTextureParameteri(m_native_texture, GL_TEXTURE_WRAP_S, address_mode_u);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_WRAP_S!", Destroy());
        glTextureParameteri(m_native_texture, GL_TEXTURE_WRAP_T, address_mode_v);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_WRAP_T!", Destroy());
        glTextureParameteri(m_native_texture, GL_TEXTURE_WRAP_R, address_mode_w);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_WRAP_R!", Destroy());
        glTextureParameterfv(m_native_texture, GL_TEXTURE_BORDER_COLOR, m_sampler_desc.border_color.data);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_BORDER_COLOR!", Destroy());
        glTextureParameteri(m_native_texture, GL_TEXTURE_BASE_LEVEL, m_sampler_desc.base_mip_level);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_BASE_LEVEL!", Destroy());
        const i32 max_mip_level = m_sampler_desc.max_mip_level == 0 ? m_max_mip_levels : m_sampler_desc.max_mip_level;
        glTextureParameteri(m_native_texture, GL_TEXTURE_MAX_LEVEL, max_mip_level);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_MAX_LEVEL!", Destroy());
        glTextureParameterf(m_native_texture, GL_TEXTURE_LOD_BIAS, m_sampler_desc.lod_bias);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_LOD_BIAS!", Destroy());
        glTextureParameterf(m_native_texture, GL_TEXTURE_MIN_LOD, m_sampler_desc.min_lod);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_MIN_LOD!", Destroy());
        glTextureParameterf(m_native_texture, GL_TEXTURE_MAX_LOD, m_sampler_desc.max_lod);
        RNDR_GL_VERIFY("Failed to set texture parameter GL_TEXTURE_MAX_LOD!", Destroy());
    }

    const GLenum internal_format = FromPixelFormatToInternalFormat(m_texture_desc.pixel_format);
    const GLenum format = FromPixelFormatToExternalFormat(m_texture_desc.pixel_format);
    const GLenum data_type = FromPixelFormatToDataType(m_texture_desc.pixel_format);

    if (!init_data.IsEmpty() && m_texture_desc.sample_count > 1)
    {
        RNDR_LOG_WARNING("Initial data is ignored for multi-sample textures!");
    }

    // Setup texture storage for 2D texture
    if (m_texture_desc.type == TextureType::Texture2D)
    {
        if (m_texture_desc.sample_count == 1)
        {
            glTextureStorage2D(m_native_texture, m_max_mip_levels, internal_format, m_texture_desc.width, m_texture_desc.height);
            RNDR_GL_VERIFY("Failed to set texture storage for Texture 2D!", Destroy());
        }
        else
        {
            glTextureStorage2DMultisample(m_native_texture, m_texture_desc.sample_count, internal_format, m_texture_desc.width,
                                          m_texture_desc.height, GL_TRUE);
            RNDR_GL_VERIFY("Failed to set multisample texture storage for Texture 2D!", Destroy());
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (!init_data.IsEmpty() && m_texture_desc.sample_count == 1)
        {
            constexpr int32_t k_y_offset = 0;
            constexpr int32_t k_x_offset = 0;
            constexpr int32_t k_mip_level = 0;
            glTextureSubImage2D(m_native_texture, k_mip_level, k_x_offset, k_y_offset, m_texture_desc.width, m_texture_desc.height, format,
                                data_type, init_data.GetData());
            RNDR_GL_VERIFY("Failed to upload initial texture data to the GPU!", Destroy());
        }
    }

    if (m_texture_desc.type == TextureType::Texture2DArray)
    {
        if (m_texture_desc.sample_count == 1)
        {
            glTextureStorage3D(m_native_texture, m_max_mip_levels, internal_format, m_texture_desc.width, m_texture_desc.height,
                               m_texture_desc.array_size);
            RNDR_GL_VERIFY("Failed to set texture storage for Texture 2D Array!", Destroy());
        }
        else
        {
            glTextureStorage3DMultisample(m_native_texture, m_texture_desc.sample_count, internal_format, m_texture_desc.width,
                                          m_texture_desc.height, m_texture_desc.array_size, GL_TRUE);
            RNDR_GL_VERIFY("Failed to set multisample texture storage for Texture 2D Array!", Destroy());
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (!init_data.IsEmpty() && m_texture_desc.sample_count == 1)
        {
            for (int i = 0; i < m_texture_desc.array_size; i++)
            {
                constexpr int32_t k_mip_level = 0;
                constexpr int32_t k_x_offset = 0;
                constexpr int32_t k_y_offset = 0;
                constexpr int32_t k_depth = 1;
                const int32_t z_offset = i;
                const i32 pixel_size = FromPixelFormatToPixelSize(m_texture_desc.pixel_format);
                const uint8_t* data = init_data.GetData() + i * m_texture_desc.width * m_texture_desc.height * pixel_size;
                glTextureSubImage3D(m_native_texture, k_mip_level, k_x_offset, k_y_offset, z_offset, m_texture_desc.width,
                                    m_texture_desc.height, k_depth, format, data_type, data);
                RNDR_GL_VERIFY("Failed to upload initial texture data to the GPU!", Destroy());
            }
        }
    }

    if (m_texture_desc.type == TextureType::CubeMap)
    {
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        RNDR_GL_VERIFY("Failed to enable seamless cubemap texture sampling!", Destroy());

        if (m_texture_desc.sample_count == 1)
        {
            glTextureStorage2D(m_native_texture, m_max_mip_levels, internal_format, m_texture_desc.width, m_texture_desc.height);
            RNDR_GL_VERIFY("Failed to set texture storage for CubeMap!", Destroy());
        }
        else
        {
            glTextureStorage2DMultisample(m_native_texture, m_texture_desc.sample_count, internal_format, m_texture_desc.width,
                                          m_texture_desc.height, GL_TRUE);
            RNDR_GL_VERIFY("Failed to set multisample texture storage for CubeMap!", Destroy());
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (!init_data.IsEmpty() && m_texture_desc.sample_count == 1)
        {
            constexpr int k_face_count = 6;
            for (int i = 0; i < k_face_count; i++)
            {
                constexpr int32_t k_mip_level = 0;
                constexpr int32_t k_x_offset = 0;
                constexpr int32_t k_y_offset = 0;
                constexpr int32_t k_depth = 1;
                const int32_t z_offset = i;
                const uint8_t* data = init_data.GetData() + i * m_texture_desc.width * m_texture_desc.height *
                                                                FromPixelFormatToPixelSize(m_texture_desc.pixel_format);
                glTextureSubImage3D(m_native_texture, k_mip_level, k_x_offset, k_y_offset, z_offset, m_texture_desc.width,
                                    m_texture_desc.height, k_depth, format, data_type, data);
                RNDR_GL_VERIFY("Failed to upload initial texture data to the GPU!", Destroy());
            }
        }
    }

    if (m_texture_desc.use_mips)
    {
        if (m_texture_desc.sample_count == 1)
        {
            glGenerateTextureMipmap(m_native_texture);
            RNDR_GL_VERIFY("Failed to generate mips!", Destroy());
        }
        else
        {
            RNDR_LOG_WARNING("Mipmaps are not supported for multi-sample textures!");
        }
    }

    // Create bindless texture on the GPU
    if (graphics_context.GetDesc().enable_bindless_textures && m_texture_desc.is_bindless)
    {
        m_bindless_handle = glGetTextureHandleARB(m_native_texture);
        RNDR_GL_VERIFY("Failed to get bindless texture handle!", Destroy());
        glMakeTextureHandleResidentARB(m_bindless_handle);
        RNDR_GL_VERIFY("Failed to make texture handle resident!", Destroy());
    }

    return ErrorCode::Success;
}

Rndr::Texture::~Texture()
{
    Destroy();
}

Rndr::Texture::Texture(Texture&& other) noexcept
    : m_texture_desc(other.m_texture_desc),
      m_sampler_desc(other.m_sampler_desc),
      m_native_texture(other.m_native_texture),
      m_max_mip_levels(other.m_max_mip_levels)
{
    other.m_native_texture = k_invalid_opengl_object;
}

Rndr::Texture& Rndr::Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_texture_desc = other.m_texture_desc;
        m_sampler_desc = other.m_sampler_desc;
        m_native_texture = other.m_native_texture;
        m_max_mip_levels = other.m_max_mip_levels;
        other.m_native_texture = k_invalid_opengl_object;
    }
    return *this;
}

void Rndr::Texture::Destroy()
{
    if (m_native_texture != k_invalid_opengl_object)
    {
        glDeleteTextures(1, &m_native_texture);
        m_native_texture = k_invalid_opengl_object;
    }
}

bool Rndr::Texture::IsValid() const
{
    return m_native_texture != k_invalid_opengl_object;
}

const Rndr::TextureDesc& Rndr::Texture::GetTextureDesc() const
{
    return m_texture_desc;
}

const Rndr::SamplerDesc& Rndr::Texture::GetSamplerDesc() const
{
    return m_sampler_desc;
}

GLuint Rndr::Texture::GetNativeTexture() const
{
    return m_native_texture;
}

uint64_t Rndr::Texture::GetBindlessHandle() const
{
    return m_bindless_handle;
}
