#include "rndr/core/definitions.h"

#if RNDR_OPENGL

#if RNDR_WINDOWS

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#undef near
#undef far

#endif  // RNDR_WINDOWS

#include <glad/glad.h>
#include <glad/glad_wgl.h>

#include "core/platform/opengl-helpers.h"
#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/file.h"
#include "rndr/core/platform/opengl-graphics-context.h"
#include "rndr/core/platform/opengl-render-api.h"
#include "rndr/utility/cpu-tracer.h"

#define RNDR_ASSERT_GL_ERROR() RNDR_ASSERT(glGetError() == GL_NO_ERROR)

///////////////////////////////////////////////////////////////////////////////////////////////////

Rndr::SwapChain::SwapChain(const Rndr::GraphicsContext& graphics_context, const Rndr::SwapChainDesc& desc) : m_desc(desc)
{
    RNDR_UNUSED(graphics_context);
    wglSwapIntervalEXT(desc.enable_vsync ? 1 : 0);
}

const Rndr::SwapChainDesc& Rndr::SwapChain::GetDesc() const
{
    return m_desc;
}

bool Rndr::SwapChain::IsValid() const
{
    return true;
}

bool Rndr::SwapChain::SetSize(int32_t width, int32_t height)
{
    if (width <= 0 || height <= 0)
    {
        RNDR_LOG_ERROR("Invalid swap chain size!");
        return false;
    }
    m_desc.width = width;
    m_desc.height = height;
    return true;
}

bool Rndr::SwapChain::SetVerticalSync(bool vertical_sync)
{
    m_desc.enable_vsync = vertical_sync;
    wglSwapIntervalEXT(vertical_sync ? 1 : 0);
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Rndr::Shader::Shader(const GraphicsContext& graphics_context, const ShaderDesc& desc) : m_desc(desc)
{
    RNDR_TRACE_SCOPED(Create Shader);

    RNDR_UNUSED(graphics_context);
    const GLenum shader_type = Rndr::FromShaderTypeToOpenGL(desc.type);
    m_native_shader = glCreateShader(shader_type);
    if (m_native_shader == k_invalid_opengl_object)
    {
        return;
    }
    const char* source = desc.source.c_str();
    glShaderSource(m_native_shader, 1, &source, nullptr);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to set shader source!");
        Destroy();
        return;
    }
    glCompileShader(m_native_shader);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to compile shader source!");
        Destroy();
        return;
    }
}

Rndr::Shader::~Shader()
{
    Destroy();
}

Rndr::Shader::Shader(Rndr::Shader&& other) noexcept : m_desc(std::move(other.m_desc)), m_native_shader(other.m_native_shader)
{
    other.m_native_shader = k_invalid_opengl_object;
}

Rndr::Shader& Rndr::Shader::operator=(Rndr::Shader&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_native_shader = other.m_native_shader;
        other.m_native_shader = k_invalid_opengl_object;
    }
    return *this;
}

void Rndr::Shader::Destroy()
{
    if (m_native_shader != k_invalid_opengl_object)
    {
        glDeleteShader(m_native_shader);
        m_native_shader = k_invalid_opengl_object;
    }
}

bool Rndr::Shader::IsValid() const
{
    return m_native_shader != k_invalid_opengl_object;
}

const Rndr::ShaderDesc& Rndr::Shader::GetDesc() const
{
    return m_desc;
}
const GLuint Rndr::Shader::GetNativeShader() const
{
    return m_native_shader;
}

///////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////

Rndr::Buffer::Buffer(const GraphicsContext& graphics_context, const BufferDesc& desc, const ConstByteSpan& init_data) : m_desc(desc)
{
    RNDR_TRACE_SCOPED(Create Buffer);

    RNDR_UNUSED(graphics_context);
    glCreateBuffers(1, &m_native_buffer);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to create buffer!");
        return;
    }
    const GLenum buffer_usage = FromUsageToOpenGL(desc.usage);
    glNamedBufferStorage(m_native_buffer, desc.size, init_data.data(), buffer_usage);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to initialize buffer!");
        Destroy();
        return;
    }
    RNDR_LOG_DEBUG("Created %s buffer %u, size: %d, stride: %d, usage %s", FromBufferTypeToString(m_desc.type).c_str(), m_native_buffer,
                   desc.size, m_desc.stride, FromOpenGLUsageToString(buffer_usage).c_str());
}

Rndr::Buffer::~Buffer()
{
    Destroy();
}

Rndr::Buffer::Buffer(Buffer&& other) noexcept : m_desc(other.m_desc), m_native_buffer(other.m_native_buffer)
{
    other.m_native_buffer = k_invalid_opengl_object;
}

Rndr::Buffer& Rndr::Buffer::operator=(Buffer&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_native_buffer = other.m_native_buffer;
        other.m_native_buffer = k_invalid_opengl_object;
    }
    return *this;
}

void Rndr::Buffer::Destroy()
{
    if (m_native_buffer != k_invalid_opengl_object)
    {
        glDeleteBuffers(1, &m_native_buffer);
        m_native_buffer = k_invalid_opengl_object;
    }
}

bool Rndr::Buffer::IsValid() const
{
    return m_native_buffer != k_invalid_opengl_object;
}

const Rndr::BufferDesc& Rndr::Buffer::GetDesc() const
{
    return m_desc;
}

GLuint Rndr::Buffer::GetNativeBuffer() const
{
    return m_native_buffer;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Rndr::Image::Image(const GraphicsContext& graphics_context, const ImageDesc& desc, const ConstByteSpan& init_data) : m_desc(desc)
{
    RNDR_TRACE_SCOPED(Create Image);

    RNDR_UNUSED(graphics_context);

    // TODO: Add support for multi sample images
    constexpr bool k_is_multi_sample = false;
    const GLenum target = FromImageInfoToTarget(desc.type, k_is_multi_sample);
    glCreateTextures(target, 1, &m_native_texture);
    const GLint min_filter = desc.use_mips ? FromMinAndMipFiltersToOpenGL(desc.sampler.min_filter, desc.sampler.mip_map_filter)
                                           : FromImageFilterToOpenGL(desc.sampler.min_filter);
    const GLint mag_filter = FromImageFilterToOpenGL(desc.sampler.mag_filter);
    glTextureParameteri(m_native_texture, GL_TEXTURE_MIN_FILTER, min_filter);
    glTextureParameteri(m_native_texture, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTextureParameterf(m_native_texture, GL_TEXTURE_MAX_ANISOTROPY, desc.sampler.max_anisotropy);
    const GLint address_mode_u = FromImageAddressModeToOpenGL(desc.sampler.address_mode_u);
    const GLint address_mode_v = FromImageAddressModeToOpenGL(desc.sampler.address_mode_v);
    const GLint address_mode_w = FromImageAddressModeToOpenGL(desc.sampler.address_mode_w);
    glTextureParameteri(m_native_texture, GL_TEXTURE_WRAP_S, address_mode_u);
    glTextureParameteri(m_native_texture, GL_TEXTURE_WRAP_T, address_mode_v);
    glTextureParameteri(m_native_texture, GL_TEXTURE_WRAP_R, address_mode_w);
    glTextureParameterfv(m_native_texture, GL_TEXTURE_BORDER_COLOR, desc.sampler.border_color.data);
    glTextureParameteri(m_native_texture, GL_TEXTURE_BASE_LEVEL, desc.sampler.base_mip_level);
    glTextureParameteri(m_native_texture, GL_TEXTURE_MAX_LEVEL, desc.sampler.max_mip_level);
    glTextureParameterf(m_native_texture, GL_TEXTURE_LOD_BIAS, desc.sampler.lod_bias);
    glTextureParameterf(m_native_texture, GL_TEXTURE_MIN_LOD, desc.sampler.min_lod);
    glTextureParameterf(m_native_texture, GL_TEXTURE_MAX_LOD, desc.sampler.max_lod);
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
    }
    if (desc.type == ImageType::Image2D)
    {
        const GLenum internal_format = FromPixelFormatToInternalFormat(desc.pixel_format);
        const GLenum format = FromPixelFormatToExternalFormat(desc.pixel_format);
        const GLenum data_type = FromPixelFormatToDataType(desc.pixel_format);
        glTextureStorage2D(m_native_texture, mip_map_levels, internal_format, desc.width, desc.height);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        constexpr int32_t k_mip_level = 0;
        constexpr int32_t k_x_offset = 0;
        constexpr int32_t k_y_offset = 0;
        if (!init_data.empty())
        {
            glTextureSubImage2D(m_native_texture, k_mip_level, k_x_offset, k_y_offset, desc.width, desc.height, format, data_type,
                                init_data.data());
            RNDR_ASSERT_GL_ERROR();
        }
        if (desc.use_mips)
        {
            glGenerateTextureMipmap(m_native_texture);
            RNDR_ASSERT_GL_ERROR();
        }
    }
    else if (desc.type == ImageType::CubeMap)
    {
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        const GLenum internal_format = FromPixelFormatToInternalFormat(desc.pixel_format);
        const GLenum format = FromPixelFormatToExternalFormat(desc.pixel_format);
        const GLenum data_type = FromPixelFormatToDataType(desc.pixel_format);
        glTextureStorage2D(m_native_texture, mip_map_levels, internal_format, desc.width, desc.height);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
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
            RNDR_ASSERT_GL_ERROR();
        }
        if (desc.use_mips)
        {
            glGenerateTextureMipmap(m_native_texture);
            RNDR_ASSERT_GL_ERROR();
        }
    }
    else
    {
        assert(false && "Not implemented yet!");
    }

    if (m_desc.is_bindless)
    {
        m_bindless_handle = glGetTextureHandleARB(m_native_texture);
        RNDR_ASSERT_GL_ERROR();
        glMakeTextureHandleResidentARB(m_bindless_handle);
        RNDR_ASSERT_GL_ERROR();
    }

    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to initialize image!");
        Destroy();
        return;
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

///////////////////////////////////////////////////////////////////////////////////////////////////

#endif  // RNDR_OPENGL
