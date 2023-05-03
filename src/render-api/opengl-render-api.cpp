#include "rndr/core/definitions.h"

#if RNDR_OPENGL

#if RNDR_WINDOWS
#include <Windows.h>
#endif  // RNDR_WINDOWS

#include <glad/glad.h>
#include <glad/glad_wgl.h>

#include "render-api/opengl-helpers.h"
#include "rndr/core/file.h"
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
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to set clear color!");
        return false;
    }
    glClear(GL_COLOR_BUFFER_BIT);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to clear color buffer!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::ClearColorAndDepth(const math::Vector4& color, real depth)
{
    glClearColor(color.X, color.Y, color.Z, color.W);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to set clear color!");
        return false;
    }
    glClearDepth(depth);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to set clear depth!");
        return false;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to clear color and depth buffers!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::Bind(const Rndr::SwapChain& swap_chain)
{
    RNDR_UNUSED(swap_chain);
    const SwapChainDesc& desc = swap_chain.GetDesc();
    glViewport(0, 0, desc.width, desc.height);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to set viewport!");
        return false;
    }
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
    const Rndr::PipelineDesc desc = pipeline.GetDesc();
    if (desc.depth_stencil.is_depth_enabled)
    {
        glEnable(GL_DEPTH_TEST);
        const GLenum depth_func = FromComparatorToOpenGL(desc.depth_stencil.depth_comparator);
        glDepthFunc(depth_func);
        glDepthMask(desc.depth_stencil.depth_mask == Rndr::DepthMask::All ? GL_TRUE : GL_FALSE);
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to set depth test state!");
            return false;
        }
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to disable depth test!");
            return false;
        }
    }
    if (desc.depth_stencil.is_stencil_enabled)
    {
        constexpr uint32_t k_mask_all_enabled = 0xFFFFFFFF;
        glEnable(GL_STENCIL_TEST);
        glStencilMask(desc.depth_stencil.stencil_write_mask);
        const GLenum front_face_stencil_func =
            FromComparatorToOpenGL(desc.depth_stencil.stencil_front_face_comparator);
        glStencilFuncSeparate(GL_FRONT,
                              front_face_stencil_func,
                              desc.depth_stencil.stencil_ref_value,
                              k_mask_all_enabled);
        const GLenum back_face_stencil_func =
            FromComparatorToOpenGL(desc.depth_stencil.stencil_back_face_comparator);
        glStencilFuncSeparate(GL_BACK,
                              back_face_stencil_func,
                              desc.depth_stencil.stencil_ref_value,
                              k_mask_all_enabled);
        const GLenum front_face_stencil_fail_op =
            FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_fail_op);
        const GLenum front_face_stencil_depth_fail_op =
            FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_depth_fail_op);
        const GLenum front_face_stencil_pass_op =
            FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_pass_op);
        glStencilOpSeparate(GL_FRONT,
                            front_face_stencil_fail_op,
                            front_face_stencil_depth_fail_op,
                            front_face_stencil_pass_op);
        const GLenum back_face_stencil_fail_op =
            FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_fail_op);
        const GLenum back_face_stencil_depth_fail_op =
            FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_depth_fail_op);
        const GLenum back_face_stencil_pass_op =
            FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_pass_op);
        glStencilOpSeparate(GL_BACK,
                            back_face_stencil_fail_op,
                            back_face_stencil_depth_fail_op,
                            back_face_stencil_pass_op);
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to set stencil test state!");
            return false;
        }
    }
    else
    {
        glDisable(GL_STENCIL_TEST);
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to disable stencil test!");
            return false;
        }
    }
    if (desc.blend.is_enabled)
    {
        glEnable(GL_BLEND);
        const GLenum src_color_factor = FromBlendFactorToOpenGL(desc.blend.src_color_factor);
        const GLenum dst_color_factor = FromBlendFactorToOpenGL(desc.blend.dst_color_factor);
        const GLenum src_alpha_factor = FromBlendFactorToOpenGL(desc.blend.src_alpha_factor);
        const GLenum dst_alpha_factor = FromBlendFactorToOpenGL(desc.blend.dst_alpha_factor);
        const GLenum color_op = FromBlendOperationToOpenGL(desc.blend.color_operation);
        const GLenum alpha_op = FromBlendOperationToOpenGL(desc.blend.alpha_operation);
        glBlendFuncSeparate(src_color_factor, dst_color_factor, src_alpha_factor, dst_alpha_factor);
        glBlendEquationSeparate(color_op, alpha_op);
        glBlendColor(desc.blend.const_color.R,
                     desc.blend.const_color.G,
                     desc.blend.const_color.B,
                     desc.blend.const_alpha);
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to set blending state!");
            return false;
        }
    }
    else
    {
        glDisable(GL_BLEND);
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to disable blending!");
            return false;
        }
    }
    // Rasterizer configuration
    {
        glPolygonMode(GL_FRONT_AND_BACK,
                      desc.rasterizer.fill_mode == FillMode::Solid ? GL_FILL : GL_LINE);
        if (desc.rasterizer.cull_face != Face::None)
        {
            glEnable(GL_CULL_FACE);
            glCullFace(desc.rasterizer.cull_face == Face::Front ? GL_FRONT : GL_BACK);
        }
        else
        {
            glDisable(GL_CULL_FACE);
        }
        glFrontFace(desc.rasterizer.front_face_winding_order == WindingOrder::CW ? GL_CW : GL_CCW);
        if (desc.rasterizer.depth_bias != MATH_REALC(0.0)
            || desc.rasterizer.slope_scaled_depth_bias != MATH_REALC(0.0))
        {
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(desc.rasterizer.slope_scaled_depth_bias, desc.rasterizer.depth_bias);
        }
        else
        {
            glDisable(GL_POLYGON_OFFSET_LINE);
        }
        if (desc.rasterizer.scissor_size.X > 0 && desc.rasterizer.scissor_size.Y > 0)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(static_cast<int32_t>(desc.rasterizer.scissor_bottom_left.X),
                      static_cast<int32_t>(desc.rasterizer.scissor_bottom_left.Y),
                      static_cast<int32_t>(desc.rasterizer.scissor_size.X),
                      static_cast<int32_t>(desc.rasterizer.scissor_size.Y));
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to set rasterizer state!");
            return false;
        }
    }
    if (pipeline.GetDesc().input_layout.index_buffer.IsValid())
    {
        if (!Bind(*pipeline.GetDesc().input_layout.index_buffer, 0))
        {
            RNDR_LOG_ERROR("Failed to bind index buffer!");
            return false;
        }
    }
    m_bound_pipeline = &pipeline;
    return true;
}

bool Rndr::GraphicsContext::Bind(const Buffer& buffer, int32_t binding_index)
{
    const GLuint native_buffer = buffer.GetNativeBuffer();
    const BufferDesc& desc = buffer.GetDesc();
    const GLenum target = FromBufferTypeToOpenGL(desc.type);
    if (target == GL_UNIFORM_BUFFER)
    {
        glBindBufferRange(GL_UNIFORM_BUFFER, binding_index, native_buffer, 0, desc.size);
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to bind uniform buffer!");
            return false;
        }
        return true;
    }
    if (target == GL_ELEMENT_ARRAY_BUFFER)
    {
        glBindBuffer(target, native_buffer);
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to bind index buffer!");
            return false;
        }
        return true;
    }
    assert(false && "Unsupported buffer type!");
    return false;
}

bool Rndr::GraphicsContext::Bind(const Image& image)
{
    const GLuint native_texture = image.GetNativeTexture();
    glBindTextures(0, 1, &native_texture);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to bind image!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::DrawVertices(PrimitiveTopology topology,
                                         int32_t vertex_count,
                                         int32_t instance_count,
                                         int32_t first_vertex)
{
    const GLenum primitive = FromPrimitiveTopologyToOpenGL(topology);
    glDrawArraysInstanced(primitive, first_vertex, vertex_count, instance_count);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to draw!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::DrawIndices(PrimitiveTopology topology,
                                        int32_t index_count,
                                        int32_t instance_count,
                                        int32_t first_index)
{
    assert(m_bound_pipeline != nullptr);
    assert(m_bound_pipeline->IsIndexBufferBound());

    const uint32_t index_size = m_bound_pipeline->GetIndexBufferElementSize();
    const GLenum index_size_enum = FromIndexSizeToOpenGL(index_size);
    const size_t index_offset = index_size * first_index;
    void* index_start = reinterpret_cast<void*>(index_offset);
    const GLenum primitive = FromPrimitiveTopologyToOpenGL(topology);
    glDrawElementsInstanced(primitive, index_count, index_size_enum, index_start, instance_count);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to draw!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::Update(Buffer& buffer, const ByteSpan& data, uint32_t offset)
{
    const GLuint native_buffer = buffer.GetNativeBuffer();
    glNamedBufferSubData(native_buffer, offset, static_cast<GLsizeiptr>(data.size()), data.data());
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to update buffer!");
        return false;
    }
    return true;
}

Rndr::CPUImage Rndr::GraphicsContext::ReadSwapChain(const SwapChain& swap_chain)
{
    const int32_t width = swap_chain.GetDesc().width;
    const int32_t height = swap_chain.GetDesc().height;
    const int32_t size = width * height * 4;
    ByteArray data(size);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to read swap chain!");
        return {};
    }
    for (int32_t i = 0; i < height / 2; i++)
    {
        for (int32_t j = 0; j < width; j++)
        {
            const int32_t index1 = i * width * 4 + j * 4;
            const int32_t index2 = (height - i - 1) * width * 4 + j * 4;
            std::swap(data[index1], data[index2]);
            std::swap(data[index1 + 1], data[index2 + 1]);
            std::swap(data[index1 + 2], data[index2 + 2]);
            std::swap(data[index1 + 3], data[index2 + 3]);
        }
    }
    return CPUImage{width, height, PixelFormat::R8G8B8A8_UNORM_SRGB, data};
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
    glBindVertexArray(m_native_vertex_array);
    for (int i = 0; i < desc.input_layout.vertex_buffers.size(); i++)
    {
        const Buffer& buffer = desc.input_layout.vertex_buffers[i].Get();
        const BufferDesc& buffer_desc = buffer.GetDesc();
        if (buffer_desc.type == BufferType::Vertex)
        {
            const int32_t binding_index = desc.input_layout.vertex_buffer_binding_slots[i];
            glVertexArrayVertexBuffer(m_native_vertex_array,
                                      binding_index,
                                      buffer.GetNativeBuffer(),
                                      buffer_desc.offset,
                                      buffer_desc.stride);
        }
    }
    for (int i = 0; i < desc.input_layout.elements.size(); i++)
    {
        const InputLayoutElement& element = desc.input_layout.elements[i];
        const int32_t attribute_index = i;
        constexpr GLboolean k_should_normalize_data = GL_FALSE;
        glEnableVertexArrayAttrib(m_native_vertex_array, attribute_index);
        glVertexArrayAttribFormat(m_native_vertex_array,
                                  attribute_index,
                                  FromPixelFormatToComponentCount(element.format),
                                  FromPixelFormatToDataType(element.format),
                                  k_should_normalize_data,
                                  element.offset_in_vertex);
        glVertexArrayAttribBinding(m_native_vertex_array, attribute_index, element.binding_index);
        if (element.repetition == DataRepetition::PerInstance)
        {
            glVertexArrayBindingDivisor(m_native_vertex_array,
                                        element.binding_index,
                                        element.instance_step_rate);
        }
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to set vertex array attribute!");
            Destroy();
            return;
        }
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

bool Rndr::Pipeline::IsIndexBufferBound() const
{
    return m_desc.input_layout.index_buffer.IsValid();
}

uint32_t Rndr::Pipeline::GetIndexBufferElementSize() const
{
    assert(IsIndexBufferBound());
    const Buffer& index_buffer = *m_desc.input_layout.index_buffer;
    const BufferDesc& index_buffer_desc = index_buffer.GetDesc();
    return index_buffer_desc.stride;
}

Rndr::Buffer::Buffer(const GraphicsContext& graphics_context,
                     const BufferDesc& desc,
                     const ByteSpan& init_data)
    : m_desc(desc)
{
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
}

Rndr::Buffer::~Buffer()
{
    Destroy();
}

Rndr::Buffer::Buffer(Buffer&& other) noexcept
    : m_desc(other.m_desc), m_native_buffer(other.m_native_buffer)
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

Rndr::Image::Image(const GraphicsContext& graphics_context,
                   const ImageDesc& desc,
                   const ByteSpan& init_data)
    : m_desc(desc)
{
    RNDR_UNUSED(graphics_context);
    assert(desc.type == ImageType::Image2D);

    const GLenum target = FromImageInfoToTarget(desc.type, desc.use_mips);
    glCreateTextures(target, 1, &m_native_texture);
    const GLint min_filter = desc.use_mips ? FromImageFilterToMinFilter(desc.sampler.min_filter,
                                                                        desc.sampler.mip_map_filter)
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
    glTextureParameterfv(m_native_texture, GL_TEXTURE_BORDER_COLOR, desc.sampler.border_color.Data);
    glTextureParameteri(m_native_texture, GL_TEXTURE_BASE_LEVEL, desc.sampler.base_mip_level);
    glTextureParameteri(m_native_texture, GL_TEXTURE_MAX_LEVEL, desc.sampler.max_mip_level);
    glTextureParameterf(m_native_texture, GL_TEXTURE_LOD_BIAS, desc.sampler.lod_bias);
    glTextureParameterf(m_native_texture, GL_TEXTURE_MIN_LOD, desc.sampler.min_lod);
    glTextureParameterf(m_native_texture, GL_TEXTURE_MAX_LOD, desc.sampler.max_lod);
    // TODO(Marko): Left to handle GL_DEPTH_STENCIL_TEXTURE_MODE, GL_TEXTURE_COMPARE_FUNC,
    // GL_TEXTURE_COMPARE_MODE

    math::Vector2 size{static_cast<math::real>(desc.width), static_cast<math::real>(desc.height)};
    int mip_map_levels = 1;
    if (desc.use_mips)
    {
        mip_map_levels += static_cast<int>(math::Floor(math::Log2(math::Max(size.X, size.Y))));
    }
    if (desc.type == ImageType::Image2D)
    {
        const GLenum internal_format = FromPixelFormatToInternalFormat(desc.pixel_format);
        const GLenum format = FromPixelFormatToFormat(desc.pixel_format);
        const GLenum data_type = FromPixelFormatToDataType(desc.pixel_format);
        glTextureStorage2D(m_native_texture,
                           mip_map_levels,
                           internal_format,
                           desc.width,
                           desc.height);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        constexpr int32_t k_mip_level = 0;
        constexpr int32_t k_x_offset = 0;
        constexpr int32_t k_y_offset = 0;
        glTextureSubImage2D(m_native_texture,
                            k_mip_level,
                            k_x_offset,
                            k_y_offset,
                            desc.width,
                            desc.height,
                            format,
                            data_type,
                            init_data.data());
    }
    else
    {
        assert(false && "Not implemented yet!");
    }
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to initialize image!");
        Destroy();
        return;
    }
}

Rndr::Image::Image(const GraphicsContext& graphics_context,
                   CPUImage& cpu_image,
                   bool use_mips,
                   const SamplerDesc& sampler_desc)
    : Image(graphics_context,
            ImageDesc{.width = cpu_image.width,
                      .height = cpu_image.height,
                      .type = ImageType::Image2D,
                      .pixel_format = cpu_image.pixel_format,
                      .use_mips = use_mips,
                      .sampler = sampler_desc},
            ByteSpan(cpu_image.data))
{
}

Rndr::Image::~Image()
{
    Destroy();
}

Rndr::Image::Image(Image&& other) noexcept
    : m_desc(other.m_desc), m_native_texture(other.m_native_texture)
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

#endif  // RNDR_OPENGL
