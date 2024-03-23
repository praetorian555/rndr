#include "rndr/core/platform/opengl-graphics-context.h"

#include "rndr/core/platform/windows-header.h"

#include <glad/glad.h>
#include <glad/glad_wgl.h>

#include "core/platform/opengl-helpers.h"
#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/containers/array.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/containers/hash-map.h"
#include "rndr/core/file.h"
#include "rndr/core/platform/opengl-render-api.h"
#include "rndr/utility/cpu-tracer.h"

#define RNDR_ASSERT_GL_ERROR() RNDR_ASSERT(glGetError() == GL_NO_ERROR)

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
            //        case GL_DEBUG_SEVERITY_LOW:
            //            RNDR_LOG_INFO("[OpenGL] %s", message);
            //            break;
            //        case GL_DEBUG_SEVERITY_NOTIFICATION:
            //            RNDR_LOG_DEBUG("[OpenGL] %s", message);
            //            break;
        default:
            break;
    }
}

bool CheckRequiredExtensions(const Rndr::Array<Rndr::String>& required_extensions)
{
    int extension_count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);
    Rndr::HashSet<Rndr::String> found_extensions;
    for (int i = 0; i < extension_count; i++)
    {
        const char* extension = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        for (const Rndr::String& required_extension : required_extensions)
        {
            if (required_extension == extension)
            {
                found_extensions.insert(required_extension);
                break;
            }
        }
    }

    const bool success = found_extensions.size() == required_extensions.size();
    return success;
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

    const Array<String> required_extensions = {
        "ARB_texture_storage",
        "ARB_multi_draw_indirect",
        "ARB_buffer_storage",
        "ARB_enhanced_layouts",
        "ARB_direct_state_access",
        "GL_ARB_indirect_parameters",
        "GL_ARB_shader_draw_parameters",
        "ARB_gl_spirv"
    };

    if (CheckRequiredExtensions(required_extensions))
    {
        RNDR_HALT("Not all required extensions are available, exiting!");
    }
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
      m_native_graphics_context(other.m_native_graphics_context),
      m_bound_pipeline(std::move(other.m_bound_pipeline))
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
        m_bound_pipeline = std::move(other.m_bound_pipeline);
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

bool Rndr::GraphicsContext::ClearDepth(float depth)
{
    RNDR_TRACE_SCOPED(Clear Depth);

    glClearDepth(depth);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to set clear depth!");
        return false;
    }
    glClear(GL_DEPTH_BUFFER_BIT);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to clear depth buffer!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::ClearStencil(int32_t stencil)
{
    RNDR_TRACE_SCOPED(Clear Stencil);

    glClearStencil(stencil);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to set clear stencil!");
        return false;
    }
    glClear(GL_STENCIL_BUFFER_BIT);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to clear stencil buffer!");
        return false;
    }
    return true;
}

bool Rndr::GraphicsContext::ClearAll(const Vector4f& color, float depth, int32_t stencil)
{
    RNDR_TRACE_SCOPED(Clear All);

    glClearColor(color.x, color.y, color.z, color.w);
    glClearDepth(depth);
    glClearStencil(stencil);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to set clear color, depth and stencil!");
        return false;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to clear color, depth and stencil buffers!");
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
        glBindBufferRange(target, binding_index, native_buffer, 0, desc.size);
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

bool Rndr::GraphicsContext::BindImageForCompute(const Rndr::Image& image, int32_t binding_index, int32_t image_level,
                                                Rndr::ImageAccess access)
{
    RNDR_TRACE_SCOPED(Bind Image For Compute);

    glBindImageTexture(binding_index, image.GetNativeTexture(), image_level, GL_FALSE, 0, FromImageAccessToOpenGL(access),
                       FromPixelFormatToInternalFormat(image.GetDesc().pixel_format));
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to bind image for compute!");
        return false;
    }
    return false;
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

bool Rndr::GraphicsContext::DispatchCompute(uint32_t block_count_x, uint32_t block_count_y, uint32_t block_count_z,
                                            bool wait_for_completion)
{
    RNDR_TRACE_SCOPED(Dispatch Compute);

    glDispatchCompute(block_count_x, block_count_y, block_count_z);
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to dispatch compute!");
        return false;
    }
    if (wait_for_completion)
    {
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        if (glGetError() != GL_NO_ERROR)
        {
            RNDR_LOG_ERROR("Failed to wait for compute completion!");
            return false;
        }
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

bool Rndr::GraphicsContext::Read(const Rndr::Image& image, Rndr::Bitmap& out_data, int32_t level) const
{
    RNDR_TRACE_SCOPED(Read Image Contents);

    if (!image.IsValid())
    {
        RNDR_LOG_ERROR("Read of the image failed since the image is invalid!");
        return false;
    }

    const ImageDesc& desc = image.GetDesc();
    const GLenum format = FromPixelFormatToExternalFormat(desc.pixel_format);
    const GLenum data_type = FromPixelFormatToDataType(desc.pixel_format);
    const int32_t pixel_size = FromPixelFormatToPixelSize(desc.pixel_format);
    Array<uint8_t> tmp_data(pixel_size * desc.width * desc.height);
    glGetTextureImage(image.GetNativeTexture(), level, format, data_type, pixel_size * desc.width * desc.height, tmp_data.data());
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to read image!");
        return false;
    }

    out_data = Bitmap(desc.width, desc.height, 1, desc.pixel_format, tmp_data);
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

bool Rndr::GraphicsContext::ReadSwapChainColor(const SwapChain& swap_chain, Bitmap& out_bitmap)
{
    RNDR_TRACE_SCOPED(Read SwapChain);

    const int32_t width = swap_chain.GetDesc().width;
    const int32_t height = swap_chain.GetDesc().height;
    const int32_t size = width * height * 4;
    ByteArray data(size);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to read swap chain!");
        return false;
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
    out_bitmap = Bitmap{width, height, 1, PixelFormat::R8G8B8A8_UNORM_SRGB, data};
    return true;
}

bool Rndr::GraphicsContext::ReadSwapChainDepthStencil(const SwapChain& swap_chain, Bitmap& out_bitmap)
{
    RNDR_TRACE_SCOPED(Read SwapChain);

    const int32_t width = swap_chain.GetDesc().width;
    const int32_t height = swap_chain.GetDesc().height;
    const int32_t size = width * height;
    Array<uint32_t> data(size);
    glReadPixels(0, 0, width, height, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, data.data());
    if (glGetError() != GL_NO_ERROR)
    {
        RNDR_LOG_ERROR("Failed to read swap chain!");
        return false;
    }
    const ByteSpan byte_data(reinterpret_cast<uint8_t*>(data.data()), size * sizeof(uint32_t));
    out_bitmap = Bitmap{width, height, 1, PixelFormat::D24_UNORM_S8_UINT, byte_data};
    return true;
}
