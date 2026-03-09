#include "rndr/canvas/texture.hpp"

#include "glad/glad.h"

#include "rndr/exception.hpp"
#include "rndr/trace.hpp"

namespace
{

struct GLFormatInfo
{
    GLenum internal_format;
    GLenum format;
    GLenum type;
    Rndr::i32 pixel_size;
};

GLFormatInfo ToGLFormat(Rndr::Canvas::Format format)
{
    switch (format)
    {
        case Rndr::Canvas::Format::R8:
            return {GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1};
        case Rndr::Canvas::Format::RG8:
            return {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2};
        case Rndr::Canvas::Format::RGB8:
            return {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 3};
        case Rndr::Canvas::Format::RGBA8:
            return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
        case Rndr::Canvas::Format::R16F:
            return {GL_R16F, GL_RED, GL_HALF_FLOAT, 2};
        case Rndr::Canvas::Format::RG16F:
            return {GL_RG16F, GL_RG, GL_HALF_FLOAT, 4};
        case Rndr::Canvas::Format::RGBA16F:
            return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8};
        case Rndr::Canvas::Format::R32F:
            return {GL_R32F, GL_RED, GL_FLOAT, 4};
        case Rndr::Canvas::Format::RG32F:
            return {GL_RG32F, GL_RG, GL_FLOAT, 8};
        case Rndr::Canvas::Format::RGBA32F:
            return {GL_RGBA32F, GL_RGBA, GL_FLOAT, 16};
        case Rndr::Canvas::Format::D24S8:
            return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4};
        case Rndr::Canvas::Format::D32F:
            return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 4};
        default:
            return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
    }
}

GLenum ToGLTarget(Rndr::Canvas::TextureType type, bool multisample)
{
    switch (type)
    {
        case Rndr::Canvas::TextureType::Texture2D:
            return multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        case Rndr::Canvas::TextureType::Texture2DArray:
            return multisample ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
        case Rndr::Canvas::TextureType::CubeMap:
            return GL_TEXTURE_CUBE_MAP;
        default:
            return GL_TEXTURE_2D;
    }
}

GLint ToGLFilter(Rndr::Canvas::TextureFilter filter)
{
    switch (filter)
    {
        case Rndr::Canvas::TextureFilter::Nearest:
            return GL_NEAREST;
        case Rndr::Canvas::TextureFilter::Linear:
            return GL_LINEAR;
        default:
            return GL_LINEAR;
    }
}

GLint ToGLMinFilter(Rndr::Canvas::TextureFilter min_filter, Rndr::Canvas::TextureFilter mip_filter, bool use_mips)
{
    if (!use_mips)
    {
        return ToGLFilter(min_filter);
    }
    switch (min_filter)
    {
        case Rndr::Canvas::TextureFilter::Nearest:
            return mip_filter == Rndr::Canvas::TextureFilter::Nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
        case Rndr::Canvas::TextureFilter::Linear:
            return mip_filter == Rndr::Canvas::TextureFilter::Nearest ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
        default:
            return GL_LINEAR_MIPMAP_LINEAR;
    }
}

GLint ToGLWrap(Rndr::Canvas::TextureWrap wrap)
{
    switch (wrap)
    {
        case Rndr::Canvas::TextureWrap::Clamp:
            return GL_CLAMP_TO_EDGE;
        case Rndr::Canvas::TextureWrap::Border:
            return GL_CLAMP_TO_BORDER;
        case Rndr::Canvas::TextureWrap::Repeat:
            return GL_REPEAT;
        case Rndr::Canvas::TextureWrap::MirrorRepeat:
            return GL_MIRRORED_REPEAT;
        case Rndr::Canvas::TextureWrap::MirrorOnce:
            return GL_MIRROR_CLAMP_TO_EDGE;
        default:
            return GL_CLAMP_TO_EDGE;
    }
}

void ToGLBorderColor(Rndr::Canvas::BorderColor color, float out[4])
{
    switch (color)
    {
        case Rndr::Canvas::BorderColor::TransparentBlack:
            out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 0.0f;
            break;
        case Rndr::Canvas::BorderColor::OpaqueBlack:
            out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 1.0f;
            break;
        case Rndr::Canvas::BorderColor::OpaqueWhite:
            out[0] = 1.0f; out[1] = 1.0f; out[2] = 1.0f; out[3] = 1.0f;
            break;
        default:
            out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 1.0f;
            break;
    }
}

Rndr::i32 CalcMipLevels(Rndr::i32 width, Rndr::i32 height)
{
    Rndr::i32 max_dim = width > height ? width : height;
    Rndr::i32 levels = 1;
    while (max_dim > 1)
    {
        max_dim >>= 1;
        levels++;
    }
    return levels;
}

void ApplySamplerParams(GLuint handle, const Rndr::Canvas::TextureDesc& desc, Rndr::i32 max_mip_levels)
{
    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(desc.min_filter, desc.mip_map_filter, desc.use_mips));
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, ToGLFilter(desc.mag_filter));
    glTextureParameterf(handle, GL_TEXTURE_MAX_ANISOTROPY, desc.max_anisotropy);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_S, ToGLWrap(desc.wrap_u));
    glTextureParameteri(handle, GL_TEXTURE_WRAP_T, ToGLWrap(desc.wrap_v));
    glTextureParameteri(handle, GL_TEXTURE_WRAP_R, ToGLWrap(desc.wrap_w));

    float border[4];
    ToGLBorderColor(desc.border_color, border);
    glTextureParameterfv(handle, GL_TEXTURE_BORDER_COLOR, border);

    glTextureParameteri(handle, GL_TEXTURE_BASE_LEVEL, desc.base_mip_level);
    const Rndr::i32 max_level = desc.max_mip_level == 0 ? max_mip_levels : desc.max_mip_level;
    glTextureParameteri(handle, GL_TEXTURE_MAX_LEVEL, max_level);
    glTextureParameterf(handle, GL_TEXTURE_LOD_BIAS, desc.lod_bias);
    glTextureParameterf(handle, GL_TEXTURE_MIN_LOD, desc.min_lod);
    glTextureParameterf(handle, GL_TEXTURE_MAX_LOD, desc.max_lod);
}

}  // namespace

Rndr::Canvas::Texture::Texture(const Context& context, const TextureDesc& desc, const Opal::ArrayView<const u8>& init_data,
                               const Opal::StringUtf8& name)
    : m_desc(desc), m_name(name.Clone())
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Texture::Texture");
    RNDR_UNUSED(context);

    if (m_desc.width <= 0 || m_desc.height <= 0)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Texture dimensions must be positive!");
    }
    if (m_desc.type == TextureType::Texture2DArray && m_desc.array_size <= 0)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Texture2DArray must have array_size > 0!");
    }

    m_max_mip_levels = m_desc.use_mips ? CalcMipLevels(m_desc.width, m_desc.height) : 1;

    const bool multisample = m_desc.sample_count > 1;
    const GLenum target = ToGLTarget(m_desc.type, multisample);

    glCreateTextures(target, 1, &m_handle);
    if (m_handle == 0)
    {
        throw GraphicsAPIException(0, "Failed to create GL texture!");
    }

    const GLFormatInfo fmt = ToGLFormat(m_desc.format);

    // Sampler params are only valid for single-sample textures.
    if (!multisample)
    {
        ApplySamplerParams(m_handle, m_desc, m_max_mip_levels);
    }

    // Allocate storage.
    if (m_desc.type == TextureType::Texture2D)
    {
        if (multisample)
        {
            glTextureStorage2DMultisample(m_handle, m_desc.sample_count, fmt.internal_format, m_desc.width, m_desc.height, GL_TRUE);
        }
        else
        {
            glTextureStorage2D(m_handle, m_max_mip_levels, fmt.internal_format, m_desc.width, m_desc.height);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (!init_data.IsEmpty() && !multisample)
        {
            glTextureSubImage2D(m_handle, 0, 0, 0, m_desc.width, m_desc.height, fmt.format, fmt.type, init_data.GetData());
        }
    }
    else if (m_desc.type == TextureType::Texture2DArray)
    {
        if (multisample)
        {
            glTextureStorage3DMultisample(m_handle, m_desc.sample_count, fmt.internal_format, m_desc.width, m_desc.height,
                                          m_desc.array_size, GL_TRUE);
        }
        else
        {
            glTextureStorage3D(m_handle, m_max_mip_levels, fmt.internal_format, m_desc.width, m_desc.height, m_desc.array_size);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (!init_data.IsEmpty() && !multisample)
        {
            const i32 layer_size = m_desc.width * m_desc.height * fmt.pixel_size;
            for (i32 i = 0; i < m_desc.array_size; i++)
            {
                const u8* data = init_data.GetData() + i * layer_size;
                glTextureSubImage3D(m_handle, 0, 0, 0, i, m_desc.width, m_desc.height, 1, fmt.format, fmt.type, data);
            }
        }
    }
    else if (m_desc.type == TextureType::CubeMap)
    {
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        if (multisample)
        {
            glTextureStorage2DMultisample(m_handle, m_desc.sample_count, fmt.internal_format, m_desc.width, m_desc.height, GL_TRUE);
        }
        else
        {
            glTextureStorage2D(m_handle, m_max_mip_levels, fmt.internal_format, m_desc.width, m_desc.height);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (!init_data.IsEmpty() && !multisample)
        {
            constexpr i32 k_face_count = 6;
            const i32 face_size = m_desc.width * m_desc.height * fmt.pixel_size;
            for (i32 i = 0; i < k_face_count; i++)
            {
                const u8* data = init_data.GetData() + i * face_size;
                glTextureSubImage3D(m_handle, 0, 0, 0, i, m_desc.width, m_desc.height, 1, fmt.format, fmt.type, data);
            }
        }
    }

    if (m_desc.use_mips && !multisample)
    {
        glGenerateTextureMipmap(m_handle);
    }

    const GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        glDeleteTextures(1, &m_handle);
        m_handle = 0;
        throw GraphicsAPIException(err, "Failed to allocate GL texture!");
    }

    if (!m_name.IsEmpty())
    {
        glObjectLabel(GL_TEXTURE, m_handle, static_cast<GLsizei>(m_name.GetSize()), m_name.GetData());
    }
}

Rndr::Canvas::Texture::~Texture()
{
    Destroy();
}

Rndr::Canvas::Texture::Texture(Texture&& other) noexcept
    : m_desc(other.m_desc), m_handle(other.m_handle), m_max_mip_levels(other.m_max_mip_levels), m_name(std::move(other.m_name))
{
    other.m_handle = 0;
    other.m_desc = {};
    other.m_max_mip_levels = 0;
}

Rndr::Canvas::Texture& Rndr::Canvas::Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_handle = other.m_handle;
        m_max_mip_levels = other.m_max_mip_levels;
        m_name = std::move(other.m_name);
        other.m_handle = 0;
        other.m_desc = {};
        other.m_max_mip_levels = 0;
    }
    return *this;
}

Rndr::Canvas::Texture Rndr::Canvas::Texture::Clone() const
{
    if (!IsValid())
    {
        return {};
    }

    Texture clone;
    clone.m_desc = m_desc;
    clone.m_max_mip_levels = m_max_mip_levels;
    clone.m_name = m_name.Clone();

    const bool multisample = m_desc.sample_count > 1;
    const GLenum target = ToGLTarget(m_desc.type, multisample);
    const GLFormatInfo fmt = ToGLFormat(m_desc.format);

    glCreateTextures(target, 1, &clone.m_handle);

    if (!multisample)
    {
        ApplySamplerParams(clone.m_handle, m_desc, m_max_mip_levels);
    }

    if (m_desc.type == TextureType::Texture2D || m_desc.type == TextureType::CubeMap)
    {
        if (multisample)
        {
            glTextureStorage2DMultisample(clone.m_handle, m_desc.sample_count, fmt.internal_format, m_desc.width, m_desc.height, GL_TRUE);
        }
        else
        {
            glTextureStorage2D(clone.m_handle, m_max_mip_levels, fmt.internal_format, m_desc.width, m_desc.height);
        }
    }
    else if (m_desc.type == TextureType::Texture2DArray)
    {
        if (multisample)
        {
            glTextureStorage3DMultisample(clone.m_handle, m_desc.sample_count, fmt.internal_format, m_desc.width, m_desc.height,
                                          m_desc.array_size, GL_TRUE);
        }
        else
        {
            glTextureStorage3D(clone.m_handle, m_max_mip_levels, fmt.internal_format, m_desc.width, m_desc.height, m_desc.array_size);
        }
    }

    if (!clone.m_name.IsEmpty())
    {
        glObjectLabel(GL_TEXTURE, clone.m_handle, static_cast<GLsizei>(clone.m_name.GetSize()), clone.m_name.GetData());
    }

    // Determine the depth for the copy.
    i32 depth = 1;
    if (m_desc.type == TextureType::Texture2DArray)
    {
        depth = m_desc.array_size;
    }
    else if (m_desc.type == TextureType::CubeMap)
    {
        depth = 6;
    }

    glCopyImageSubData(m_handle, target, 0, 0, 0, 0, clone.m_handle, target, 0, 0, 0, 0, m_desc.width, m_desc.height, depth);

    return clone;
}

void Rndr::Canvas::Texture::Destroy()
{
    if (m_handle != 0)
    {
        glDeleteTextures(1, &m_handle);
        m_handle = 0;
        m_desc = {};
        m_max_mip_levels = 0;
    }
}

void Rndr::Canvas::Texture::Update(const Opal::ArrayView<const u8>& data) const
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Texture::Update");

    if (m_handle == 0)
    {
        throw GraphicsAPIException(0, "Cannot update an invalid texture!");
    }
    if (m_desc.sample_count > 1)
    {
        throw GraphicsAPIException(0, "Cannot update a multi-sample texture!");
    }
    if (m_desc.type != TextureType::Texture2D)
    {
        throw GraphicsAPIException(0, "Update is only supported for Texture2D!");
    }

    const GLFormatInfo fmt = ToGLFormat(m_desc.format);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(m_handle, 0, 0, 0, m_desc.width, m_desc.height, fmt.format, fmt.type, data.GetData());
}

bool Rndr::Canvas::Texture::IsValid() const
{
    return m_handle != 0;
}

const Rndr::Canvas::TextureDesc& Rndr::Canvas::Texture::GetDesc() const
{
    return m_desc;
}

const Opal::StringUtf8& Rndr::Canvas::Texture::GetName() const
{
    return m_name;
}

Rndr::u32 Rndr::Canvas::Texture::GetNativeHandle() const
{
    return m_handle;
}