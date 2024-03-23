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
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/platform/opengl-graphics-context.h"
#include "rndr/core/platform/opengl-render-api.h"
#include "rndr/utility/cpu-tracer.h"

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

///////////////////////////////////////////////////////////////////////////////////////////////////

#endif  // RNDR_OPENGL
