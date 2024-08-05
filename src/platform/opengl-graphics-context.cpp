#include "rndr/platform/opengl-graphics-context.h"

#include "glad/glad_wgl.h"

#include "opal/container/array.h"
#include "opal/container/hash-set.h"
#include "opal/container/stack-array.h"
#include "opal/container/string.h"

#include "opengl-helpers.h"
#include "rndr/file.h"
#include "rndr/log.h"
#include "rndr/platform/opengl-buffer.h"
#include "rndr/platform/opengl-frame-buffer.h"
#include "rndr/platform/opengl-pipeline.h"
#include "rndr/platform/opengl-shader.h"
#include "rndr/platform/opengl-swap-chain.h"
#include "rndr/platform/opengl-texture.h"
#include "rndr/trace.h"

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
            RNDR_LOG_ERROR("[OpenGL] %s", message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            RNDR_LOG_WARNING("[OpenGL] %s", message);
            break;
        default:
            break;
    }
}
}  // namespace

Rndr::GraphicsContext::GraphicsContext(const Rndr::GraphicsContextDesc& desc) : m_desc(desc)
{
    RNDR_CPU_EVENT_SCOPED("Create Graphics Context");

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
    const Opal::StackArray<int, 9> attribute_list = {
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
    RNDR_ASSERT_OPENGL();

    // Check if the required extensions are supported by the hardware.
    if (GLAD_GL_ARB_buffer_storage == 0)
    {
        RNDR_HALT("ARB_buffer_storage is not supported on this device, exiting!");
    }
    if (GLAD_GL_ARB_direct_state_access == 0)
    {
        RNDR_HALT("ARB_direct_state_access is not supported on this device, exiting!");
    }
    if (GLAD_GL_ARB_enhanced_layouts == 0)
    {
        RNDR_HALT("ARB_enhanced_layouts is not supported on this device, exiting!");
    }
    if (GLAD_GL_ARB_gl_spirv == 0)
    {
        RNDR_HALT("ARB_gl_spirv is not supported on this device, exiting!");
    }
    if (GLAD_GL_ARB_indirect_parameters == 0)
    {
        RNDR_HALT("ARB_indirect_parameters is not supported on this device, exiting!");
    }
    if (GLAD_GL_ARB_multi_draw_indirect == 0)
    {
        RNDR_HALT("ARB_multi_draw_indirect is not supported on this device, exiting!");
    }
    if (GLAD_GL_ARB_shader_draw_parameters == 0)
    {
        RNDR_HALT("ARB_shader_draw_parameters is not supported on this device, exiting!");
    }
    if (GLAD_GL_ARB_texture_storage == 0)
    {
        RNDR_HALT("ARB_texture_storage is not supported on this device, exiting!");
    }
    if (m_desc.enable_bindless_textures && GLAD_GL_ARB_gpu_shader_int64 == 0)
    {
        RNDR_HALT("ARB_gpu_shader_int64 is not supported on this device, exiting!");
    }
    if (m_desc.enable_bindless_textures && GLAD_GL_ARB_bindless_texture == 0)
    {
        RNDR_HALT("ARB_bindless_texture is not supported on this device, exiting!");
    }
#else
    RNDR_ASSERT(false && "OS not supported!");
#endif  // RNDR_WINDOWS
}

Rndr::GraphicsContext::~GraphicsContext()
{
    Destroy();
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

void Rndr::GraphicsContext::Destroy()
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

const Rndr::GraphicsContextDesc& Rndr::GraphicsContext::GetDesc() const
{
    return m_desc;
}

bool Rndr::GraphicsContext::Present(const Rndr::SwapChain& swap_chain)
{
    RNDR_CPU_EVENT_SCOPED("Present");

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
    RNDR_CPU_EVENT_SCOPED("Clear Color");

    glClearColor(color.x, color.y, color.z, color.w);
    RNDR_ASSERT_OPENGL();
    glClear(GL_COLOR_BUFFER_BIT);
    RNDR_ASSERT_OPENGL();
    return true;
}

bool Rndr::GraphicsContext::ClearDepth(float depth)
{
    RNDR_CPU_EVENT_SCOPED("Clear Depth");

    glClearDepth(depth);
    RNDR_ASSERT_OPENGL();
    glClear(GL_DEPTH_BUFFER_BIT);
    RNDR_ASSERT_OPENGL();
    return true;
}

bool Rndr::GraphicsContext::ClearStencil(i32 stencil)
{
    RNDR_CPU_EVENT_SCOPED("Clear Stencil");

    glClearStencil(stencil);
    RNDR_ASSERT_OPENGL();
    glClear(GL_STENCIL_BUFFER_BIT);
    RNDR_ASSERT_OPENGL();
    return true;
}

bool Rndr::GraphicsContext::ClearAll(const Vector4f& color, float depth, i32 stencil)
{
    RNDR_CPU_EVENT_SCOPED("Clear All");

    glClearColor(color.x, color.y, color.z, color.w);
    RNDR_ASSERT_OPENGL();
    glClearDepth(depth);
    RNDR_ASSERT_OPENGL();
    glClearStencil(stencil);
    RNDR_ASSERT_OPENGL();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    RNDR_ASSERT_OPENGL();
    return true;
}

bool Rndr::GraphicsContext::Bind(const Rndr::SwapChain& swap_chain)
{
    RNDR_CPU_EVENT_SCOPED("Bind SwapChain");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    RNDR_ASSERT_OPENGL();
    const SwapChainDesc& desc = swap_chain.GetDesc();
    glViewport(0, 0, desc.width, desc.height);
    RNDR_ASSERT_OPENGL();
    return true;
}

bool Rndr::GraphicsContext::Bind(const Pipeline& pipeline)
{
    RNDR_CPU_EVENT_SCOPED("Bind Pipeline");

    const GLuint shader_program = pipeline.GetNativeShaderProgram();
    glUseProgram(shader_program);
    RNDR_ASSERT_OPENGL();
    const GLuint vertex_array = pipeline.GetNativeVertexArray();
    glBindVertexArray(vertex_array);
    RNDR_ASSERT_OPENGL();
    const Rndr::PipelineDesc desc = pipeline.GetDesc();
    if (desc.depth_stencil.is_depth_enabled)
    {
        glEnable(GL_DEPTH_TEST);
        RNDR_ASSERT_OPENGL();
        const GLenum depth_func = FromComparatorToOpenGL(desc.depth_stencil.depth_comparator);
        glDepthFunc(depth_func);
        RNDR_ASSERT_OPENGL();
        glDepthMask(desc.depth_stencil.depth_mask == Rndr::DepthMask::All ? GL_TRUE : GL_FALSE);
        RNDR_ASSERT_OPENGL();
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
        RNDR_ASSERT_OPENGL();
    }
    if (desc.depth_stencil.is_stencil_enabled)
    {
        constexpr u32 k_mask_all_enabled = 0xFFFFFFFF;
        glEnable(GL_STENCIL_TEST);
        RNDR_ASSERT_OPENGL();
        glStencilMask(desc.depth_stencil.stencil_write_mask);
        RNDR_ASSERT_OPENGL();
        const GLenum front_face_stencil_func = FromComparatorToOpenGL(desc.depth_stencil.stencil_front_face_comparator);
        glStencilFuncSeparate(GL_FRONT, front_face_stencil_func, desc.depth_stencil.stencil_ref_value, k_mask_all_enabled);
        RNDR_ASSERT_OPENGL();
        const GLenum back_face_stencil_func = FromComparatorToOpenGL(desc.depth_stencil.stencil_back_face_comparator);
        glStencilFuncSeparate(GL_BACK, back_face_stencil_func, desc.depth_stencil.stencil_ref_value, k_mask_all_enabled);
        RNDR_ASSERT_OPENGL();
        const GLenum front_face_stencil_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_fail_op);
        const GLenum front_face_stencil_depth_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_depth_fail_op);
        const GLenum front_face_stencil_pass_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_pass_op);
        glStencilOpSeparate(GL_FRONT, front_face_stencil_fail_op, front_face_stencil_depth_fail_op, front_face_stencil_pass_op);
        RNDR_ASSERT_OPENGL();
        const GLenum back_face_stencil_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_fail_op);
        const GLenum back_face_stencil_depth_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_depth_fail_op);
        const GLenum back_face_stencil_pass_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_pass_op);
        glStencilOpSeparate(GL_BACK, back_face_stencil_fail_op, back_face_stencil_depth_fail_op, back_face_stencil_pass_op);
        RNDR_ASSERT_OPENGL();
    }
    else
    {
        glDisable(GL_STENCIL_TEST);
        RNDR_ASSERT_OPENGL();
    }
    if (desc.blend.is_enabled)
    {
        glEnable(GL_BLEND);
        RNDR_ASSERT_OPENGL();
        const GLenum src_color_factor = FromBlendFactorToOpenGL(desc.blend.src_color_factor);
        const GLenum dst_color_factor = FromBlendFactorToOpenGL(desc.blend.dst_color_factor);
        const GLenum src_alpha_factor = FromBlendFactorToOpenGL(desc.blend.src_alpha_factor);
        const GLenum dst_alpha_factor = FromBlendFactorToOpenGL(desc.blend.dst_alpha_factor);
        const GLenum color_op = FromBlendOperationToOpenGL(desc.blend.color_operation);
        const GLenum alpha_op = FromBlendOperationToOpenGL(desc.blend.alpha_operation);
        glBlendFuncSeparate(src_color_factor, dst_color_factor, src_alpha_factor, dst_alpha_factor);
        RNDR_ASSERT_OPENGL();
        glBlendEquationSeparate(color_op, alpha_op);
        RNDR_ASSERT_OPENGL();
        glBlendColor(desc.blend.const_color.r, desc.blend.const_color.g, desc.blend.const_color.b, desc.blend.const_alpha);
        RNDR_ASSERT_OPENGL();
    }
    else
    {
        glDisable(GL_BLEND);
        RNDR_ASSERT_OPENGL();
    }
    // Rasterizer configuration
    {
        glPolygonMode(GL_FRONT_AND_BACK, desc.rasterizer.fill_mode == FillMode::Solid ? GL_FILL : GL_LINE);
        RNDR_ASSERT_OPENGL();
        if (desc.rasterizer.cull_face != Face::None)
        {
            glEnable(GL_CULL_FACE);
            RNDR_ASSERT_OPENGL();
            glCullFace(desc.rasterizer.cull_face == Face::Front ? GL_FRONT : GL_BACK);
            RNDR_ASSERT_OPENGL();
        }
        else
        {
            glDisable(GL_CULL_FACE);
            RNDR_ASSERT_OPENGL();
        }
        glFrontFace(desc.rasterizer.front_face_winding_order == WindingOrder::CW ? GL_CW : GL_CCW);
        RNDR_ASSERT_OPENGL();
        if (desc.rasterizer.depth_bias != 0.0f || desc.rasterizer.slope_scaled_depth_bias != 0.0f)
        {
            glEnable(GL_POLYGON_OFFSET_LINE);
            RNDR_ASSERT_OPENGL();
            glPolygonOffset(desc.rasterizer.slope_scaled_depth_bias, desc.rasterizer.depth_bias);
            RNDR_ASSERT_OPENGL();
        }
        else
        {
            glDisable(GL_POLYGON_OFFSET_LINE);
            RNDR_ASSERT_OPENGL();
        }
        if (desc.rasterizer.scissor_size.x > 0 && desc.rasterizer.scissor_size.y > 0)
        {
            glEnable(GL_SCISSOR_TEST);
            RNDR_ASSERT_OPENGL();
            glScissor(static_cast<i32>(desc.rasterizer.scissor_bottom_left.x), static_cast<i32>(desc.rasterizer.scissor_bottom_left.y),
                      static_cast<i32>(desc.rasterizer.scissor_size.x), static_cast<i32>(desc.rasterizer.scissor_size.y));
            RNDR_ASSERT_OPENGL();
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
            RNDR_ASSERT_OPENGL();
        }
    }
    m_bound_pipeline = &pipeline;
    return true;
}

bool Rndr::GraphicsContext::Bind(const Buffer& buffer, i32 binding_index)
{
    RNDR_CPU_EVENT_SCOPED("Bind Buffer");

    const GLuint native_buffer = buffer.GetNativeBuffer();
    const BufferDesc& desc = buffer.GetDesc();
    const GLenum target = FromBufferTypeToOpenGL(desc.type);
    if (target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER)
    {
        glBindBufferRange(target, binding_index, native_buffer, 0, desc.size);
        RNDR_ASSERT_OPENGL();
        return true;
    }

    RNDR_LOG_ERROR("Vertex and index buffers should be bound through the use of input layout and pipeline!");
    return false;
}

bool Rndr::GraphicsContext::Bind(const Texture& image, i32 binding_index)
{
    RNDR_CPU_EVENT_SCOPED("Bind Texture");

    const GLuint native_texture = image.GetNativeTexture();
    glBindTextures(binding_index, 1, &native_texture);
    RNDR_ASSERT_OPENGL();
    return true;
}

bool Rndr::GraphicsContext::BindImageForCompute(const Rndr::Texture& image, i32 binding_index, i32 image_level, Rndr::ImageAccess access)
{
    RNDR_CPU_EVENT_SCOPED("Bind Texture For Compute");

    glBindImageTexture(binding_index, image.GetNativeTexture(), image_level, GL_FALSE, 0, FromImageAccessToOpenGL(access),
                       FromPixelFormatToInternalFormat(image.GetTextureDesc().pixel_format));
    RNDR_ASSERT_OPENGL();
    return true;
}

bool Rndr::GraphicsContext::Bind(const Rndr::FrameBuffer& frame_buffer)
{
    RNDR_CPU_EVENT_SCOPED("Bind FrameBuffer");
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer.GetNativeFrameBuffer());
    RNDR_ASSERT_OPENGL();
    TextureDesc color_attachment_desc = frame_buffer.GetColorAttachment(0).GetTextureDesc();
    glViewport(0, 0, color_attachment_desc.width, color_attachment_desc.height);
    RNDR_ASSERT_OPENGL();
    return true;
}

bool Rndr::GraphicsContext::DrawVertices(PrimitiveTopology topology, i32 vertex_count, i32 instance_count, i32 first_vertex)
{
    RNDR_CPU_EVENT_SCOPED("Draw Vertices");

    const GLenum primitive = FromPrimitiveTopologyToOpenGL(topology);
    glDrawArraysInstanced(primitive, first_vertex, vertex_count, instance_count);
    RNDR_ASSERT_OPENGL();
    return true;
}

bool Rndr::GraphicsContext::DrawIndices(PrimitiveTopology topology, i32 index_count, i32 instance_count, i32 first_index)
{
    RNDR_CPU_EVENT_SCOPED("Draw Indices");

    RNDR_ASSERT(m_bound_pipeline.IsValid());
    RNDR_ASSERT(m_bound_pipeline->IsIndexBufferBound());

    const i64 index_size = m_bound_pipeline->GetIndexBufferElementSize();
    const GLenum index_size_enum = FromIndexSizeToOpenGL(index_size);
    const i64 index_offset = index_size * first_index;
    void* index_start = reinterpret_cast<void*>(index_offset);
    const GLenum primitive = FromPrimitiveTopologyToOpenGL(topology);
    glDrawElementsInstanced(primitive, index_count, index_size_enum, index_start, instance_count);
    RNDR_ASSERT_OPENGL();
    return true;
}

bool Rndr::GraphicsContext::DispatchCompute(u32 block_count_x, u32 block_count_y, u32 block_count_z, bool wait_for_completion)
{
    RNDR_CPU_EVENT_SCOPED("Dispatch Compute");

    glDispatchCompute(block_count_x, block_count_y, block_count_z);
    RNDR_ASSERT_OPENGL();
    if (wait_for_completion)
    {
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        RNDR_ASSERT_OPENGL();
    }
    return true;
}

Rndr::ErrorCode Rndr::GraphicsContext::UpdateBuffer(const Buffer& buffer, const Opal::Span<const u8>& data, i64 offset)
{
    RNDR_CPU_EVENT_SCOPED("Update Buffer Contents");

    if (!buffer.IsValid())
    {
        RNDR_LOG_ERROR("UpdateBuffer: Failed, invalid buffer object!");
        return ErrorCode::InvalidArgument;
    }
    const BufferDesc desc = buffer.GetDesc();
    if (desc.usage != Usage::Dynamic)
    {
        RNDR_LOG_ERROR("UpdateBuffer: Failed, buffer usage is not Usage::Dynamic!");
        return ErrorCode::InvalidArgument;
    }
    const i64 buffer_size = static_cast<i64>(desc.size);
    if (offset < 0 || offset >= buffer_size)
    {
        RNDR_LOG_ERROR("UpdateBuffer: Failed, offset %d is out of bounds [0, %u)!", offset, desc.size);
        return ErrorCode::OutOfBounds;
    }
    const i64 data_size = static_cast<i64>(data.GetSize());
    if (data_size <= 0 || offset + data_size > buffer_size)
    {
        RNDR_LOG_ERROR("UpdateBuffer: Failed, data size out of bounds!");
        return ErrorCode::OutOfBounds;
    }

    const GLuint native_buffer = buffer.GetNativeBuffer();
    glNamedBufferSubData(native_buffer, offset, data_size, data.GetData());
    const GLenum error_code = glad_glGetError();
    switch (error_code)
    {
        case GL_INVALID_VALUE:
            RNDR_LOG_ERROR("UpdateBuffer: Failed, offset or data size out of bounds!");
            return ErrorCode::InvalidArgument;
        case GL_INVALID_OPERATION:
            RNDR_LOG_ERROR("UpdateBuffer: Failed, invalid buffer object or buffer usage is not Usage::Dynamic!");
            return ErrorCode::InvalidArgument;
        default:
            return ErrorCode::Success;
    }
}

Rndr::ErrorCode Rndr::GraphicsContext::ReadBuffer(const Buffer& buffer, Opal::Span<u8>& out_data, i32 offset, i32 size) const
{
    RNDR_CPU_EVENT_SCOPED("Read Buffer Contents");

    if (!buffer.IsValid())
    {
        RNDR_LOG_ERROR("ReadBuffer: Failed, invalid buffer object!");
        return ErrorCode::InvalidArgument;
    }
    const BufferDesc& desc = buffer.GetDesc();
    if (desc.usage != Usage::ReadBack)
    {
        RNDR_LOG_ERROR("ReadBuffer: Failed, buffer usage is not Usage::ReadBack!");
        return ErrorCode::InvalidArgument;
    }

    // If size is 0, read to the end of the buffer
    if (size == 0)
    {
        size = static_cast<i32>(desc.size) - offset;
    }
    if (offset < 0 || offset >= static_cast<i32>(desc.size))
    {
        RNDR_LOG_ERROR("ReadBuffer: Failed, offset %d is out of bounds [0, %u)!", offset, desc.size);
        return ErrorCode::OutOfBounds;
    }
    if (offset + size > static_cast<i32>(desc.size))
    {
        RNDR_LOG_ERROR("ReadBuffer: Failed, read size %d results in out of bounds access!", size);
        return ErrorCode::OutOfBounds;
    }

    const GLuint native_buffer = buffer.GetNativeBuffer();
    u8* gpu_data = static_cast<u8*>(glMapNamedBufferRange(native_buffer, desc.offset, static_cast<i32>(desc.size), GL_MAP_READ_BIT));
    const GLenum error_code = glad_glGetError();
    switch (error_code)
    {
        case GL_INVALID_OPERATION:
            RNDR_LOG_ERROR(
                "ReadBuffer: Failed, either buffer is invalid object, the object is already mapped, or size of the buffer is 0!");
            return ErrorCode::InvalidArgument;
        case GL_INVALID_VALUE:
            RNDR_LOG_ERROR("ReadBuffer: Failed, buffer's offset or size are either negative or result in out of bounds access!");
            return ErrorCode::InvalidArgument;
    }

    RNDR_ASSERT(gpu_data != nullptr);
    memcpy(out_data.GetData(), gpu_data + offset, size);

    glUnmapNamedBuffer(native_buffer);
    const GLenum unmap_error_code = glad_glGetError();
    switch (unmap_error_code)
    {
        case GL_INVALID_OPERATION:
            RNDR_LOG_ERROR("ReadBuffer: Failed, buffer is not a buffer object or the object is not mapped!");
            return ErrorCode::InvalidArgument;
        default:
            return ErrorCode::Success;
    }
}

namespace
{
bool Overlap(Rndr::i32 src_offset, Rndr::i32 dst_offset, Rndr::i32 size)
{
    if (src_offset < dst_offset)
    {
        return src_offset + size > dst_offset;
    }
    if (src_offset > dst_offset)
    {
        return dst_offset + size > src_offset;
    }
    return true;
}
}  // namespace

Rndr::ErrorCode Rndr::GraphicsContext::CopyBuffer(const Rndr::Buffer& dst_buffer, const Rndr::Buffer& src_buffer, i32 dst_offset,
                                                  i32 src_offset, i32 size)
{
    RNDR_CPU_EVENT_SCOPED("Copy Buffer Contents");

    if (!dst_buffer.IsValid())
    {
        RNDR_LOG_ERROR("CopyBuffer: Failed, destination buffer is invalid!");
        return ErrorCode::InvalidArgument;
    }
    if (!src_buffer.IsValid())
    {
        RNDR_LOG_ERROR("CopyBuffer: Failed, source buffer is invalid!");
        return ErrorCode::InvalidArgument;
    }

    const BufferDesc& dst_desc = dst_buffer.GetDesc();
    const BufferDesc& src_desc = src_buffer.GetDesc();

    if (src_offset < 0 || src_offset >= static_cast<i32>(src_desc.size))
    {
        RNDR_LOG_ERROR("CopyBuffer: Failed, source offset %d is out of bounds [0, %u)!", src_offset, src_desc.size);
        return ErrorCode::OutOfBounds;
    }
    if (dst_offset < 0 || dst_offset >= static_cast<i32>(dst_desc.size))
    {
        RNDR_LOG_ERROR("CopyBuffer: Failed, destination offset %d is out of bounds [0, %u)!", dst_offset, dst_desc.size);
        return ErrorCode::OutOfBounds;
    }

    if (size == 0)
    {
        const i32 src_remaining_size = static_cast<i32>(src_desc.size) - src_offset;
        const i32 dst_remaining_size = static_cast<i32>(dst_desc.size) - dst_offset;
        size = std::min(src_remaining_size, dst_remaining_size);
    }
    if (size == 0)
    {
        RNDR_LOG_ERROR("CopyBuffer: Failed, nothing to copy!");
        return ErrorCode::InvalidArgument;
    }

    if (dst_offset + size > static_cast<i32>(dst_desc.size))
    {
        RNDR_LOG_ERROR("CopyBuffer: Failed, not enough space in the destination buffer!");
        return ErrorCode::OutOfBounds;
    }
    if (src_offset + size > static_cast<i32>(src_desc.size))
    {
        RNDR_LOG_ERROR("CopyBuffer: Failed, not enough data in the source buffer!");
        return ErrorCode::OutOfBounds;
    }
    if (src_buffer.GetNativeBuffer() == dst_buffer.GetNativeBuffer() && Overlap(src_offset, dst_offset, size))
    {
        RNDR_LOG_ERROR("CopyBuffer: Failed, source and destination buffers are the same buffer and their ranges overlap!");
        return ErrorCode::InvalidArgument;
    }

    glCopyNamedBufferSubData(src_buffer.GetNativeBuffer(), dst_buffer.GetNativeBuffer(), src_offset, dst_offset, size);
    const GLenum error_code = glad_glGetError();
    switch (error_code)
    {
        case GL_INVALID_VALUE:
            RNDR_LOG_ERROR("CopyBuffer: Failed, source and destination buffers are the same buffer and their ranges overlap!");
            return ErrorCode::InvalidArgument;
        case GL_INVALID_OPERATION:
            RNDR_LOG_ERROR(
                "CopyBuffer: Failed, either source or destination buffers are currently mapped to the CPU memory or one of these buffers "
                "are not valid!");
            return ErrorCode::InvalidArgument;
        default:
            return ErrorCode::Success;
    }
}

bool Rndr::GraphicsContext::Read(const Rndr::Texture& image, Rndr::Bitmap& out_data, i32 level) const
{
    RNDR_CPU_EVENT_SCOPED("Read Texture Contents");

    if (!image.IsValid())
    {
        RNDR_LOG_ERROR("Read of the image failed since the image is invalid!");
        return false;
    }

    const TextureDesc& desc = image.GetTextureDesc();
    const GLenum format = FromPixelFormatToExternalFormat(desc.pixel_format);
    const GLenum data_type = FromPixelFormatToDataType(desc.pixel_format);
    const i32 pixel_size = FromPixelFormatToPixelSize(desc.pixel_format);
    Opal::Array<u8> tmp_data(pixel_size * desc.width * desc.height);
    glGetTextureImage(image.GetNativeTexture(), level, format, data_type, pixel_size * desc.width * desc.height, tmp_data.GetData());
    RNDR_ASSERT_OPENGL();

    out_data = Bitmap(desc.width, desc.height, 1, desc.pixel_format, AsWritableBytes(tmp_data));
    return true;
}

bool Rndr::GraphicsContext::ReadSwapChainColor(const SwapChain& swap_chain, Bitmap& out_bitmap)
{
    RNDR_CPU_EVENT_SCOPED("Read SwapChain");

    const i32 width = swap_chain.GetDesc().width;
    const i32 height = swap_chain.GetDesc().height;
    const i32 size = width * height * 4;
    Opal::Array<u8> data(size);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.GetData());
    RNDR_ASSERT_OPENGL();
    for (i32 i = 0; i < height / 2; i++)
    {
        for (i32 j = 0; j < width; j++)
        {
            const i32 index1 = i * width * 4 + j * 4;
            const i32 index2 = (height - i - 1) * width * 4 + j * 4;
            std::swap(data[index1], data[index2]);
            std::swap(data[index1 + 1], data[index2 + 1]);
            std::swap(data[index1 + 2], data[index2 + 2]);
            std::swap(data[index1 + 3], data[index2 + 3]);
        }
    }
    out_bitmap = Bitmap{width, height, 1, PixelFormat::R8G8B8A8_UNORM_SRGB, AsWritableBytes(data)};
    return true;
}

bool Rndr::GraphicsContext::ReadSwapChainDepthStencil(const SwapChain& swap_chain, Bitmap& out_bitmap)
{
    RNDR_CPU_EVENT_SCOPED("Read SwapChain");

    const i32 width = swap_chain.GetDesc().width;
    const i32 height = swap_chain.GetDesc().height;
    const i32 size = width * height;
    Opal::Array<u32> data(size);
    glReadPixels(0, 0, width, height, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, data.GetData());
    RNDR_ASSERT_OPENGL();
    const Opal::Span<u8> byte_data(reinterpret_cast<u8*>(data.GetData()), size * sizeof(u32));
    out_bitmap = Bitmap{width, height, 1, PixelFormat::D24_UNORM_S8_UINT, byte_data};
    return true;
}
