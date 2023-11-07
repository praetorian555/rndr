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

#include "render-api/opengl-helpers.h"
#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/file.h"
#include "rndr/render-api/opengl-render-api.h"
#include "rndr/utility/cpu-tracer.h"

namespace
{
void APIENTRY DebugOutputCallback(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message,
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
            //            RNDR_HALT("Fatal error in OpenGL!");
            RNDR_LOG_ERROR("[OpenGL] %s", message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            RNDR_LOG_WARNING("[OpenGL] %s", message);
            break;
        case GL_DEBUG_SEVERITY_LOW:
            RNDR_LOG_INFO("[OpenGL] %s", message);
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            RNDR_LOG_DEBUG("[OpenGL] %s", message);
            break;
        default:
            break;
    }
}
}  // namespace

Rndr::GraphicsContext::GraphicsContext(const Rndr::GraphicsContextDesc& desc) : m_desc(desc)
{
    RNDR_TRACE_SCOPED(Create Graphics Context);

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
#if RNDR_DEBUG
    if (m_desc.enable_debug_layer)
    {
        arb_flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
    }
#endif
    const StackArray<int, 9> attribute_list = {
        WGL_CONTEXT_MAJOR_VERSION_ARB,
        4,
        WGL_CONTEXT_MINOR_VERSION_ARB,
        6,
        WGL_CONTEXT_FLAGS_ARB,
        arb_flags,
        WGL_CONTEXT_PROFILE_MASK_ARB,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0  // End of the attribute list
    };
    HGLRC old_graphics_context = graphics_context;
    graphics_context = wglCreateContextAttribsARB(m_native_device_context, graphics_context, attribute_list.data());
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

bool Rndr::GraphicsContext::Present(const Rndr::SwapChain& swap_chain)
{
    RNDR_TRACE_SCOPED(Present);

    RNDR_UNUSED(swap_chain);
    const BOOL status = SwapBuffers(m_native_device_context);
    return status == TRUE;
}

bool Rndr::GraphicsContext::IsValid() const
{
    return m_native_device_context != k_invalid_device_context_handle && m_native_graphics_context != k_invalid_graphics_context_handle;
}

bool Rndr::GraphicsContext::ClearColor(const Vector4f& color)
{
    RNDR_TRACE_SCOPED(Clear Color);

    glClearColor(color.x, color.y, color.z, color.w);
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

bool Rndr::GraphicsContext::ClearColorAndDepth(const Vector4f& color, float depth)
{
    RNDR_TRACE_SCOPED(Clear Color And Depth);

    glClearColor(color.x, color.y, color.z, color.w);
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
    RNDR_TRACE_SCOPED(Bind SwapChain);

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
    RNDR_TRACE_SCOPED(Bind Pipeline);

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
        const GLenum front_face_stencil_func = FromComparatorToOpenGL(desc.depth_stencil.stencil_front_face_comparator);
        glStencilFuncSeparate(GL_FRONT, front_face_stencil_func, desc.depth_stencil.stencil_ref_value, k_mask_all_enabled);
        const GLenum back_face_stencil_func = FromComparatorToOpenGL(desc.depth_stencil.stencil_back_face_comparator);
        glStencilFuncSeparate(GL_BACK, back_face_stencil_func, desc.depth_stencil.stencil_ref_value, k_mask_all_enabled);
        const GLenum front_face_stencil_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_fail_op);
        const GLenum front_face_stencil_depth_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_depth_fail_op);
        const GLenum front_face_stencil_pass_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_pass_op);
        glStencilOpSeparate(GL_FRONT, front_face_stencil_fail_op, front_face_stencil_depth_fail_op, front_face_stencil_pass_op);
        const GLenum back_face_stencil_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_fail_op);
        const GLenum back_face_stencil_depth_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_depth_fail_op);
        const GLenum back_face_stencil_pass_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_pass_op);
        glStencilOpSeparate(GL_BACK, back_face_stencil_fail_op, back_face_stencil_depth_fail_op, back_face_stencil_pass_op);
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
        glBlendColor(desc.blend.const_color.r, desc.blend.const_color.g, desc.blend.const_color.b, desc.blend.const_alpha);
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
        glPolygonMode(GL_FRONT_AND_BACK, desc.rasterizer.fill_mode == FillMode::Solid ? GL_FILL : GL_LINE);
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
        if (desc.rasterizer.depth_bias != 0.0f || desc.rasterizer.slope_scaled_depth_bias != 0.0f)
        {
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(desc.rasterizer.slope_scaled_depth_bias, desc.rasterizer.depth_bias);
        }
        else
        {
            glDisable(GL_POLYGON_OFFSET_LINE);
        }
        if (desc.rasterizer.scissor_size.x > 0 && desc.rasterizer.scissor_size.y > 0)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(static_cast<int32_t>(desc.rasterizer.scissor_bottom_left.x),
                      static_cast<int32_t>(desc.rasterizer.scissor_bottom_left.y), static_cast<int32_t>(desc.rasterizer.scissor_size.x),
                      static_cast<int32_t>(desc.rasterizer.scissor_size.y));
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
    m_bound_pipeline = &pipeline;
    return true;
}

bool Rndr::GraphicsContext::Bind(const Buffer& buffer, int32_t binding_index)
{
    RNDR_TRACE_SCOPED(Bind Buffer);

    const GLuint native_buffer = buffer.GetNativeBuffer();
    const BufferDesc& desc = buffer.GetDesc();
    const GLenum target = FromBufferTypeToOpenGL(desc.type);
    if (target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER)
    {
        glBindBufferRange(GL_UNIFORM_BUFFER, binding_index, native_buffer, 0, desc.size);
    }
    else
    {
        glBindBuffer(target, native_buffer);
    }
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to bind uniform buffer!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::Bind(const Image& image, int32_t binding_index)
{
    RNDR_TRACE_SCOPED(Bind Image);

    const GLuint native_texture = image.GetNativeTexture();
    glBindTextures(binding_index, 1, &native_texture);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to bind image!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::DrawVertices(PrimitiveTopology topology, int32_t vertex_count, int32_t instance_count, int32_t first_vertex)
{
    RNDR_TRACE_SCOPED(Draw Vertices);

    const GLenum primitive = FromPrimitiveTopologyToOpenGL(topology);
    glDrawArraysInstanced(primitive, first_vertex, vertex_count, instance_count);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to draw!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::DrawIndices(PrimitiveTopology topology, int32_t index_count, int32_t instance_count, int32_t first_index)
{
    RNDR_TRACE_SCOPED(Draw Indices);

    assert(m_bound_pipeline.IsValid());
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

bool Rndr::GraphicsContext::Update(const Buffer& buffer, const ConstByteSpan& data, int32_t offset)
{
    RNDR_TRACE_SCOPED(Update Buffer Contents);

    if (!buffer.IsValid())
    {
        RNDR_LOG_ERROR("Update of the buffer failed since the buffer is invalid!");
        return false;
    }

    const BufferDesc desc = buffer.GetDesc();

    if (offset < 0 || offset >= static_cast<int32_t>(desc.size))
    {
        RNDR_LOG_ERROR("Update of the buffer failed since the offset is invalid!");
        return false;
    }
    if (data.empty() || offset + static_cast<int32_t>(data.size()) > static_cast<int32_t>(desc.size))
    {
        RNDR_LOG_ERROR("Update of the buffer failed since the data size is invalid!");
        return false;
    }

    const GLuint native_buffer = buffer.GetNativeBuffer();
    glNamedBufferSubData(native_buffer, offset, static_cast<GLsizeiptr>(data.size()), data.data());
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to update buffer contents!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::Read(const Buffer& buffer, ByteSpan& out_data, int32_t offset, int32_t size) const
{
    RNDR_TRACE_SCOPED(Read Buffer Contents);

    if (!buffer.IsValid())
    {
        RNDR_LOG_ERROR("Read of the buffer failed since the buffer is invalid!");
        return false;
    }

    const BufferDesc& desc = buffer.GetDesc();
    if (size == 0)
    {
        size = static_cast<int32_t>(desc.size) - offset;
    }

    if (offset < 0 || offset >= static_cast<int32_t>(desc.size))
    {
        RNDR_LOG_ERROR("Read of the buffer failed since the offset is invalid!");
        return false;
    }
    if (size <= 0 || offset + size > static_cast<int32_t>(desc.size))
    {
        RNDR_LOG_ERROR("Read of the buffer failed since the size is invalid!");
        return false;
    }

    const GLuint native_buffer = buffer.GetNativeBuffer();
    uint8_t* gpu_data = static_cast<uint8_t*>(glMapNamedBufferRange(native_buffer, desc.offset, desc.size, GL_MAP_READ_BIT));
    if (glGetError() != GL_NO_ERROR || gpu_data == nullptr)
    {
        RNDR_LOG_ERROR("Failed to map buffer for read");
        return false;
    }

    memcpy(out_data.data(), gpu_data + offset, size);

    glUnmapNamedBuffer(native_buffer);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to unmap buffer for read");
        return false;
    }

    return true;
}

bool Rndr::GraphicsContext::Copy(const Rndr::Buffer& dst_buffer, const Rndr::Buffer& src_buffer, int32_t dst_offset, int32_t src_offset,
                                 int32_t size)
{
    RNDR_TRACE_SCOPED(Copy Buffer Contents);

    if (!dst_buffer.IsValid())
    {
        RNDR_LOG_ERROR("Copy of the buffer failed since the destination buffer is invalid!");
        return false;
    }
    if (!src_buffer.IsValid())
    {
        RNDR_LOG_ERROR("Copy of the buffer failed since the source buffer is invalid!");
        return false;
    }

    const BufferDesc& dst_desc = dst_buffer.GetDesc();
    const BufferDesc& src_desc = src_buffer.GetDesc();

    if (size == 0)
    {
        size = static_cast<int32_t>(dst_desc.size) - src_offset;
    }

    if (dst_offset < 0 || dst_offset >= static_cast<int32_t>(dst_desc.size))
    {
        RNDR_LOG_ERROR("Copy of the buffer failed since the destination offset is invalid!");
        return false;
    }
    if (src_offset < 0 || src_offset >= static_cast<int32_t>(src_desc.size))
    {
        RNDR_LOG_ERROR("Copy of the buffer failed since the source offset is invalid!");
        return false;
    }
    if (size <= 0 || dst_offset + size > static_cast<int32_t>(dst_desc.size) || src_offset + size > static_cast<int32_t>(src_desc.size))
    {
        RNDR_LOG_ERROR("Copy of the buffer failed since the size is invalid!");
        return false;
    }

    glCopyNamedBufferSubData(src_buffer.GetNativeBuffer(), dst_buffer.GetNativeBuffer(), src_offset, dst_offset, size);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to copy buffer!");
        return false;
    }
    return true;
}

Rndr::Bitmap Rndr::GraphicsContext::ReadSwapChain(const SwapChain& swap_chain)
{
    RNDR_TRACE_SCOPED(Read SwapChain);

    Bitmap invalid_bitmap{-1, -1, -1, PixelFormat::R8G8B8_UNORM_SRGB};

    const int32_t width = swap_chain.GetDesc().width;
    const int32_t height = swap_chain.GetDesc().height;
    const int32_t size = width * height * 4;
    ByteArray data(size);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to read swap chain!");
        return invalid_bitmap;
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
    return Bitmap{width, height, 1, PixelFormat::R8G8B8A8_UNORM_SRGB, data};
}

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

Rndr::Pipeline::Pipeline(const GraphicsContext& graphics_context, const PipelineDesc& desc) : m_desc(desc)
{
    RNDR_TRACE_SCOPED(Create Pipeline);

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
        glAttachShader(m_native_shader_program, desc.tesselation_evaluation_shader->GetNativeShader());
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
    GLint success = GL_FALSE;
    glGetProgramiv(m_native_shader_program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE)
    {
        constexpr size_t k_error_log_size = 1024;
        GLchar error_log[k_error_log_size] = {0};
        glGetProgramInfoLog(m_native_shader_program, k_error_log_size, nullptr, error_log);
        RNDR_LOG_ERROR("Failed to link shader program: %s", error_log);
        Destroy();
        return;
    }
    glValidateProgram(m_native_shader_program);
    glGetProgramiv(m_native_shader_program, GL_VALIDATE_STATUS, &success);
    if (success == GL_FALSE)
    {
        constexpr size_t k_error_log_size = 1024;
        GLchar error_log[k_error_log_size] = {0};
        glGetProgramInfoLog(m_native_shader_program, k_error_log_size, nullptr, error_log);
        RNDR_LOG_ERROR("Failed to link shader program: %s", error_log);
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
    const InputLayoutDesc& input_layout_desc = m_desc.input_layout;
    for (int i = 0; i < input_layout_desc.vertex_buffers.size(); i++)
    {
        const Buffer& buffer = input_layout_desc.vertex_buffers[i].Get();
        const BufferDesc& buffer_desc = buffer.GetDesc();
        const int32_t binding_index = input_layout_desc.vertex_buffer_binding_slots[i];
        if (buffer_desc.type == BufferType::Vertex)
        {
            glVertexArrayVertexBuffer(m_native_vertex_array, binding_index, buffer.GetNativeBuffer(), buffer_desc.offset,
                                      buffer_desc.stride);
            RNDR_LOG_DEBUG("Added vertex buffer %u to pipeline's vertex array buffer %u, binding index: %d, offset: %d, stride: %d",
                           buffer.GetNativeBuffer(), m_native_vertex_array, binding_index, buffer_desc.offset, buffer_desc.stride);
        }
        else if (buffer_desc.type == BufferType::ShaderStorage)
        {
            glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding_index, buffer.GetNativeBuffer(), 0, buffer_desc.size);
        }
        else
        {
            RNDR_LOG_ERROR("Invalid buffer type!");
            Destroy();
            return;
        }
    }
    for (int i = 0; i < input_layout_desc.elements.size(); i++)
    {
        const InputLayoutElement& element = input_layout_desc.elements[i];
        const int32_t attribute_index = i;
        const GLenum should_normalize_data = FromPixelFormatToShouldNormalizeData(element.format);
        const GLint component_count = FromPixelFormatToComponentCount(element.format);
        const GLenum data_type = FromPixelFormatToDataType(element.format);
        glEnableVertexArrayAttrib(m_native_vertex_array, attribute_index);
        if (IsPixelFormatInteger(element.format))
        {
            glVertexArrayAttribIFormat(m_native_vertex_array, attribute_index, component_count, data_type, element.offset_in_vertex);
        }
        else
        {
            glVertexArrayAttribFormat(m_native_vertex_array, attribute_index, component_count, data_type,
                                      static_cast<GLboolean>(should_normalize_data), element.offset_in_vertex);
        }
        glVertexArrayAttribBinding(m_native_vertex_array, attribute_index, element.binding_index);
        if (element.repetition == DataRepetition::PerInstance)
        {
            glVertexArrayBindingDivisor(m_native_vertex_array, element.binding_index, element.instance_step_rate);
        }
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to set vertex array attribute!");
            Destroy();
            return;
        }
        RNDR_LOG_DEBUG(
            "Added attribute at index %d to vertex array buffer %u, binding index: %d, component count: %d, data type: %s, should "
            "normalize data: %s, offset in vertex: %d",
            attribute_index, m_native_vertex_array, element.binding_index, component_count, FromOpenGLDataTypeToString(data_type).c_str(),
            should_normalize_data ? "GL_TRUE" : "GL_FALSE", element.offset_in_vertex);
    }
    if (input_layout_desc.index_buffer.IsValid())
    {
        const Buffer& buffer = input_layout_desc.index_buffer.Get();
        assert(buffer.GetDesc().type == BufferType::Index);
        glVertexArrayElementBuffer(m_native_vertex_array, buffer.GetNativeBuffer());
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to set vertex array index buffer!");
            Destroy();
            return;
        }
        RNDR_LOG_DEBUG("Added index buffer %u to vertex array buffer %u", buffer.GetNativeBuffer(), m_native_vertex_array);
    }
}

Rndr::Pipeline::~Pipeline()
{
    Destroy();
}

Rndr::Pipeline::Pipeline(Pipeline&& other) noexcept
    : m_desc(std::move(other.m_desc)), m_native_shader_program(other.m_native_shader_program), m_native_vertex_array(other.m_native_vertex_array)
{
    other.m_native_shader_program = k_invalid_opengl_object;
    other.m_native_vertex_array = k_invalid_opengl_object;
}

Rndr::Pipeline& Rndr::Pipeline::operator=(Pipeline&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_native_shader_program = other.m_native_shader_program;
        m_native_vertex_array = other.m_native_vertex_array;
        other.m_native_shader_program = k_invalid_opengl_object;
        other.m_native_vertex_array = k_invalid_opengl_object;
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
    return m_native_shader_program != k_invalid_opengl_object && m_native_vertex_array != k_invalid_opengl_object;
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
    // TODO(Marko): Left to handle GL_DEPTH_STENCIL_TEXTURE_MODE, GL_TEXTURE_COMPARE_FUNC,
    // GL_TEXTURE_COMPARE_MODE

    const Vector2f size{static_cast<float>(desc.width), static_cast<float>(desc.height)};
    int mip_map_levels = 1;
    if (desc.use_mips)
    {
        mip_map_levels += static_cast<int>(Math::Floor(Math::Log2(Math::Max(size.x, size.y))));
    }
    if (desc.type == ImageType::Image2D)
    {
        const GLenum internal_format = FromPixelFormatToInternalFormat(desc.pixel_format);
        const GLenum format = FromPixelFormatToFormat(desc.pixel_format);
        const GLenum data_type = FromPixelFormatToDataType(desc.pixel_format);
        glTextureStorage2D(m_native_texture, mip_map_levels, internal_format, desc.width, desc.height);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        constexpr int32_t k_mip_level = 0;
        constexpr int32_t k_x_offset = 0;
        constexpr int32_t k_y_offset = 0;
        glTextureSubImage2D(m_native_texture, k_mip_level, k_x_offset, k_y_offset, desc.width, desc.height, format, data_type,
                            init_data.data());
    }
    else if (desc.type == ImageType::CubeMap)
    {
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        const GLenum internal_format = FromPixelFormatToInternalFormat(desc.pixel_format);
        const GLenum format = FromPixelFormatToFormat(desc.pixel_format);
        const GLenum data_type = FromPixelFormatToDataType(desc.pixel_format);
        glTextureStorage2D(m_native_texture, mip_map_levels, internal_format, desc.width, desc.height);
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
        }
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

///////////////////////////////////////////////////////////////////////////////////////////////////

struct CommandExecutor
{
    Rndr::Ref<Rndr::GraphicsContext> graphics_context;

    void operator()(const Rndr::BindSwapChainCommand& command) const { graphics_context->Bind(command.swap_chain); }

    void operator()(const Rndr::BindPipelineCommand& command) const { graphics_context->Bind(command.pipeline); }

    void operator()(const Rndr::BindConstantBufferCommand& command) const
    {
        graphics_context->Bind(command.constant_buffer, command.binding_index);
    }

    void operator()(const Rndr::BindImageCommand& command) const { graphics_context->Bind(command.image, command.binding_index); }

    void operator()(const Rndr::DrawVerticesCommand& command) const
    {
        graphics_context->DrawVertices(command.primitive_topology, command.vertex_count, command.instance_count, command.first_vertex);
    }

    void operator()(const Rndr::DrawIndicesCommand& command) const
    {
        graphics_context->DrawIndices(command.primitive_topology, command.index_count, command.instance_count, command.first_index);
    }

    void operator()(const Rndr::DrawVerticesMultiCommand& command) const
    {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, command.buffer_handle);
        const GLenum topology = FromPrimitiveTopologyToOpenGL(command.primitive_topology);
        glMultiDrawArraysIndirect(topology, nullptr, static_cast<int32_t>(command.draw_count), sizeof(Rndr::DrawVerticesData));
    }

    void operator()(const Rndr::DrawIndicesMultiCommand& command) const
    {
        const GLenum topology = FromPrimitiveTopologyToOpenGL(command.primitive_topology);
        glMultiDrawElementsIndirect(topology, GL_UNSIGNED_INT, nullptr, static_cast<int32_t>(command.draw_count),
                                    sizeof(Rndr::DrawIndicesData));
    }
};

Rndr::CommandList::CommandList(Rndr::GraphicsContext& graphics_context) : m_graphics_context(graphics_context) {}

void Rndr::CommandList::Bind(const Rndr::SwapChain& swap_chain)
{
    m_commands.emplace_back(BindSwapChainCommand{.swap_chain = Ref<const SwapChain>(swap_chain)});
}

void Rndr::CommandList::Bind(const Rndr::Pipeline& pipeline)
{
    m_commands.emplace_back(BindPipelineCommand{.pipeline = Ref<const Pipeline>(pipeline)});
}

void Rndr::CommandList::BindConstantBuffer(const Rndr::Buffer& buffer, int32_t binding_index)
{
    m_commands.emplace_back(BindConstantBufferCommand{.constant_buffer = Ref<const Buffer>(buffer), .binding_index = binding_index});
}

void Rndr::CommandList::Bind(const Rndr::Image& image, int32_t binding_index)
{
    m_commands.emplace_back(BindImageCommand{.image = Ref<const Image>(image), .binding_index = binding_index});
}

void Rndr::CommandList::DrawVertices(Rndr::PrimitiveTopology topology, int32_t vertex_count, int32_t instance_count, int32_t first_vertex)
{
    m_commands.emplace_back(DrawVerticesCommand{
        .primitive_topology = topology, .vertex_count = vertex_count, .instance_count = instance_count, .first_vertex = first_vertex});
}

void Rndr::CommandList::DrawIndices(Rndr::PrimitiveTopology topology, int32_t index_count, int32_t instance_count, int32_t first_index)
{
    m_commands.emplace_back(DrawIndicesCommand{
        .primitive_topology = topology, .index_count = index_count, .instance_count = instance_count, .first_index = first_index});
}

void Rndr::CommandList::DrawVerticesMulti(const Rndr::Pipeline& pipeline, Rndr::PrimitiveTopology topology,
                                          const Rndr::Span<Rndr::DrawVerticesData>& draws)
{
    GLuint buffer_handle = 0;
    glCreateBuffers(1, &buffer_handle);
    RNDR_ASSERT(glGetError() == GL_NO_ERROR);
    glNamedBufferStorage(buffer_handle, draws.size() * sizeof(DrawVerticesData), draws.data(), GL_DYNAMIC_STORAGE_BIT);
    RNDR_ASSERT(glGetError() == GL_NO_ERROR);

    Bind(pipeline);
    m_commands.emplace_back(DrawVerticesMultiCommand(topology, buffer_handle, static_cast<uint32_t>(draws.size())));
}

void Rndr::CommandList::DrawIndicesMulti(const Rndr::Pipeline& pipeline, Rndr::PrimitiveTopology topology,
                                         const Rndr::Span<Rndr::DrawIndicesData>& draws)
{
    m_graphics_context->Bind(pipeline);

    GLuint buffer_handle = 0;
    glCreateBuffers(1, &buffer_handle);
    RNDR_ASSERT(glGetError() == GL_NO_ERROR);
    glNamedBufferStorage(buffer_handle, draws.size() * sizeof(DrawIndicesData), draws.data(), GL_DYNAMIC_STORAGE_BIT);
    RNDR_ASSERT(glGetError() == GL_NO_ERROR);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer_handle);
    RNDR_ASSERT(glGetError() == GL_NO_ERROR);

    Bind(pipeline);
    m_commands.emplace_back(DrawIndicesMultiCommand(topology, buffer_handle, static_cast<uint32_t>(draws.size())));
}

void Rndr::CommandList::Submit()
{
    RNDR_TRACE_SCOPED(Submit Command List);

    for (const auto& command : m_commands)
    {
        std::visit(CommandExecutor{m_graphics_context}, command);
    }
}

#endif  // RNDR_OPENGL
