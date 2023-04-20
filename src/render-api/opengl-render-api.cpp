#include "rndr/core/definitions.h"

#if RNDR_OPENGL

#if RNDR_WINDOWS
#include <Windows.h>
#endif  // RNDR_WINDOWS

#include <glad/glad.h>
#include <glad/glad_wgl.h>

#include "render-api/opengl-helpers.h"
#include "rndr/core/stack-array.h"
#include "rndr/render-api/opengl-render-api.h"

namespace
{
void APIENTRY DebugOutputCallback(GLenum source,
                                  GLenum type,
                                  unsigned int id,
                                  GLenum severity,
                                  GLsizei length,
                                  const char* message,
                                  const void* user_param)
{
    RNDR_UNUSED(source);
    RNDR_UNUSED(type);
    RNDR_UNUSED(id);
    RNDR_UNUSED(length);
    RNDR_UNUSED(user_param);
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:
            RNDR_LOG_ERROR("[OpenGL] %s", message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            RNDR_LOG_WARNING("[OpenGL] %s", message);
            break;
        case GL_DEBUG_SEVERITY_LOW:
            RNDR_LOG_INFO("[OpenGL] %s", message);
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            RNDR_LOG_TRACE("[OpenGL] %s", message);
            break;
        default:
            break;
    }
}
}  // namespace

Rndr::GraphicsContext::GraphicsContext(const Rndr::GraphicsContextDesc& desc) : m_desc(desc)
{
#if RNDR_WINDOWS
    if (m_desc.window_handle == nullptr)
    {
        RNDR_LOG_ERROR("Window handle is null!");
        return;
    }

    m_native_device_context = GetDC(m_desc.window_handle);
    if (m_native_device_context == nullptr)
    {
        RNDR_LOG_ERROR("Failed to get device context from a native window!");
        return;
    }

    PIXELFORMATDESCRIPTOR pixel_format_desc;
    memset(&pixel_format_desc, 0, sizeof(PIXELFORMATDESCRIPTOR));
    pixel_format_desc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pixel_format_desc.nVersion = 1;
    pixel_format_desc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixel_format_desc.iPixelType = PFD_TYPE_RGBA;
    pixel_format_desc.cColorBits = 32;
    pixel_format_desc.cAlphaBits = 8;
    pixel_format_desc.cDepthBits = 24;
    pixel_format_desc.cStencilBits = 8;

    const int pixel_format = ChoosePixelFormat(m_native_device_context, &pixel_format_desc);
    if (pixel_format == 0)
    {
        RNDR_LOG_ERROR("Chosen pixel format does not exist!");
        return;
    }
    BOOL status = SetPixelFormat(m_native_device_context, pixel_format, &pixel_format_desc);
    if (status == 0)
    {
        RNDR_LOG_ERROR("Failed to set new pixel format to the device context!");
        return;
    }

    HGLRC graphics_context = wglCreateContext(m_native_device_context);
    if (graphics_context == nullptr)
    {
        RNDR_LOG_ERROR("Failed to create OpenGL graphics context!");
        return;
    }

    status = wglMakeCurrent(m_native_device_context, graphics_context);
    if (status == 0)
    {
        RNDR_LOG_ERROR("Failed to make OpenGL graphics context current!");
        return;
    }

    status = gladLoadWGL(m_native_device_context);
    if (status == 0)
    {
        RNDR_LOG_ERROR("Failed to load WGL functions!");
        return;
    }

    int arb_flags = WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
    if (m_desc.enable_debug_layer)
    {
        arb_flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
    }
    const StackArray<int, 9> attribute_list = {
        WGL_CONTEXT_MAJOR_VERSION_ARB,
        m_desc.gl_major_version,
        WGL_CONTEXT_MINOR_VERSION_ARB,
        m_desc.gl_minor_version,
        WGL_CONTEXT_FLAGS_ARB,
        arb_flags,
        WGL_CONTEXT_PROFILE_MASK_ARB,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0  // End of the attribute list
    };
    HGLRC old_graphics_context = graphics_context;
    graphics_context = wglCreateContextAttribsARB(m_native_device_context,
                                                  graphics_context,
                                                  attribute_list.data());
    if (graphics_context == nullptr)
    {
        RNDR_LOG_ERROR("Failed to make OpenGL graphics context with attribute list!");
        return;
    }

    status = wglMakeCurrent(m_native_device_context, graphics_context);
    if (status == 0)
    {
        RNDR_LOG_ERROR("Failed to make OpenGL graphics context current!");
        return;
    }

    status = wglDeleteContext(old_graphics_context);
    if (status == 0)
    {
        RNDR_LOG_ERROR("Failed to delete temporary graphics context!");
        return;
    }

    status = gladLoadGL();
    if (status == 0)
    {
        RNDR_LOG_ERROR("Failed to load OpenGL functions!");
        return;
    }

    m_native_graphics_context = graphics_context;

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&DebugOutputCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#else
    assert(false && "OS not supported!");
#endif  // RNDR_WINDOWS
}

Rndr::GraphicsContext::~GraphicsContext()
{
#if RNDR_WINDOWS
    if (m_native_graphics_context != k_invalid_graphics_context_handle)
    {
        const BOOL status = wglDeleteContext(m_native_graphics_context);
        if (status == 0)
        {
            RNDR_LOG_ERROR("Failed to destroy OpenGL graphics context!");
        }
    }
    if (m_native_device_context != k_invalid_device_context_handle)
    {
        const BOOL status = ReleaseDC(m_desc.window_handle, m_native_device_context);
        if (status == 0)
        {
            RNDR_LOG_ERROR("Failed to release device context!");
        }
    }
#endif  // RNDR_WINDOWS
}

Rndr::GraphicsContext::GraphicsContext(Rndr::GraphicsContext&& other) noexcept
    : m_desc(other.m_desc),
      m_native_device_context(other.m_native_device_context),
      m_native_graphics_context(other.m_native_graphics_context)
{
    other.m_native_device_context = nullptr;
    other.m_native_graphics_context = nullptr;
}

Rndr::GraphicsContext& Rndr::GraphicsContext::operator=(Rndr::GraphicsContext&& other) noexcept
{
    if (this != &other)
    {
        m_desc = other.m_desc;
        this->~GraphicsContext();
        m_native_device_context = other.m_native_device_context;
        m_native_graphics_context = other.m_native_graphics_context;
        other.m_native_device_context = nullptr;
        other.m_native_graphics_context = nullptr;
    }
    return *this;
}

const Rndr::GraphicsContextDesc& Rndr::GraphicsContext::GetDesc() const
{
    return m_desc;
}

bool Rndr::GraphicsContext::Present(const Rndr::SwapChain& swap_chain, bool vertical_sync)
{
    RNDR_UNUSED(swap_chain);
    RNDR_UNUSED(vertical_sync);
    const BOOL status = SwapBuffers(m_native_device_context);
    return status == TRUE;
}

bool Rndr::GraphicsContext::IsValid() const
{
    return m_native_device_context != k_invalid_device_context_handle
           && m_native_graphics_context != k_invalid_graphics_context_handle;
}

bool Rndr::GraphicsContext::ClearColor(const math::Vector4& color)
{
    glClearColor(color.X, color.Y, color.Z, color.W);
    glClear(GL_COLOR_BUFFER_BIT);
    return true;
}

bool Rndr::GraphicsContext::Bind(const Rndr::SwapChain& swap_chain)
{
    RNDR_UNUSED(swap_chain);
    const SwapChainDesc& desc = swap_chain.GetDesc();
    glViewport(0, 0, desc.width, desc.height);
    return true;
}

bool Rndr::GraphicsContext::Bind(const Pipeline& pipeline)
{
    const GLuint shader_program = pipeline.GetNativeShaderProgram();
    glUseProgram(shader_program);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to bind pipeline's shader program!");
        return false;
    }
    const GLuint vertex_array = pipeline.GetNativeVertexArray();
    glBindVertexArray(vertex_array);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to bind pipeline's vertex array!");
        return false;
    }
    // TODO(Marko): Configure pipeline state here based on the pipeline desc.
    return true;
}

bool Rndr::GraphicsContext::Draw(uint32_t vertex_count,
                                 uint32_t instance_count,
                                 uint32_t first_vertex,
                                 uint32_t first_instance)
{
    RNDR_UNUSED(first_instance);
    glDrawArraysInstanced(GL_TRIANGLES, first_vertex, vertex_count, instance_count);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to draw!");
        return false;
    }
    return true;
}

Rndr::SwapChain::SwapChain(const Rndr::GraphicsContext& graphics_context,
                           const Rndr::SwapChainDesc& desc)
    : m_desc(desc)
{
    RNDR_UNUSED(graphics_context);
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

Rndr::Shader::Shader(const GraphicsContext& graphics_context, const ShaderDesc& desc) : m_desc(desc)
{
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

Rndr::Shader::Shader(Rndr::Shader&& other) noexcept : m_native_shader(other.m_native_shader)
{
    other.m_native_shader = k_invalid_opengl_object;
}

Rndr::Shader& Rndr::Shader::operator=(Rndr::Shader&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
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

Rndr::Pipeline::Pipeline(const GraphicsContext& graphics_context, const PipelineDesc& desc)
    : m_desc(desc)
{
    RNDR_UNUSED(graphics_context);
    m_native_shader_program = glCreateProgram();
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to create shader program!");
        return;
    }

    if (desc.vertex_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.vertex_shader->GetNativeShader());
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to attach vertex shader!");
            Destroy();
            return;
        }
    }
    if (desc.pixel_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.pixel_shader->GetNativeShader());
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to attach pixel shader!");
            Destroy();
            return;
        }
    }
    if (desc.geometry_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.geometry_shader->GetNativeShader());
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to attach geometry shader!");
            Destroy();
            return;
        }
    }
    if (desc.tesselation_control_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.tesselation_control_shader->GetNativeShader());
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to attach tesselation control shader!");
            Destroy();
            return;
        }
    }
    if (desc.tesselation_evaluation_shader != nullptr)
    {
        glAttachShader(m_native_shader_program,
                       desc.tesselation_evaluation_shader->GetNativeShader());
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to attach tesselation evaluation shader!");
            Destroy();
            return;
        }
    }
    if (desc.compute_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.compute_shader->GetNativeShader());
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to attach compute shader!");
            Destroy();
            return;
        }
    }
    glLinkProgram(m_native_shader_program);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to link shader program!");
        Destroy();
        return;
    }
    glCreateVertexArrays(1, &m_native_vertex_array);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to create vertex array!");
        Destroy();
        return;
    }
}

Rndr::Pipeline::~Pipeline()
{
    Destroy();
}

Rndr::Pipeline::Pipeline(Pipeline&& other) noexcept
    : m_native_shader_program(other.m_native_shader_program)
{
    other.m_native_shader_program = k_invalid_opengl_object;
}

Rndr::Pipeline& Rndr::Pipeline::operator=(Pipeline&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_native_shader_program = other.m_native_shader_program;
        other.m_native_shader_program = k_invalid_opengl_object;
    }
    return *this;
}

void Rndr::Pipeline::Destroy()
{
    if (m_native_shader_program != k_invalid_opengl_object)
    {
        glDeleteProgram(m_native_shader_program);
        m_native_shader_program = k_invalid_opengl_object;
    }
    if (m_native_vertex_array != k_invalid_opengl_object)
    {
        glDeleteVertexArrays(1, &m_native_vertex_array);
        m_native_vertex_array = k_invalid_opengl_object;
    }
}

bool Rndr::Pipeline::IsValid() const
{
    return m_native_shader_program != k_invalid_opengl_object;
}

const Rndr::PipelineDesc& Rndr::Pipeline::GetDesc() const
{
    return m_desc;
}

GLuint Rndr::Pipeline::GetNativeShaderProgram() const
{
    return m_native_shader_program;
}

GLuint Rndr::Pipeline::GetNativeVertexArray() const
{
    return m_native_vertex_array;
}

#endif  // RNDR_OPENGL
