#include "rndr/platform/opengl-graphics-context.hpp"

#include "glad/glad_wgl.h"

#include "opal/common.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/hash-set.h"
#include "opal/container/in-place-array.h"
#include "opal/container/string.h"

#include "opengl-helpers.hpp"
#include "rndr/file.hpp"
#include "rndr/log.hpp"
#include "rndr/platform/opengl-buffer.hpp"
#include "rndr/platform/opengl-frame-buffer.hpp"
#include "rndr/platform/opengl-pipeline.hpp"
#include "rndr/platform/opengl-shader.hpp"
#include "rndr/platform/opengl-swap-chain.hpp"
#include "rndr/platform/opengl-texture.hpp"
#include "rndr/return-macros.hpp"
#include "rndr/trace.hpp"

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

Rndr::GraphicsContext::GraphicsContext(const GraphicsContextDesc& desc)
{
    Init(desc);
}

Rndr::GraphicsContext::~GraphicsContext()
{
    Destroy();
}

Rndr::GraphicsContext::GraphicsContext(GraphicsContext&& other) noexcept
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

Rndr::ErrorCode Rndr::GraphicsContext::Init(const GraphicsContextDesc& desc)
{
    RNDR_CPU_EVENT_SCOPED("GraphicsContext::Init");

#if RNDR_WINDOWS
    RNDR_RETURN_ON_FAIL(desc.window_handle != nullptr, ErrorCode::InvalidArgument, "Window handle is null!", RNDR_NOOP);

    m_native_device_context = GetDC(RNDR_TO_HWND(desc.window_handle));
    RNDR_RETURN_ON_FAIL(m_native_device_context != nullptr, ErrorCode::PlatformError, "Failed to get device context from a native window!",
                        RNDR_NOOP);

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
    RNDR_RETURN_ON_FAIL(pixel_format != 0, ErrorCode::PlatformError, "Pixel format RGBA is not supported!", Destroy());

    BOOL status = SetPixelFormat(m_native_device_context, pixel_format, &pixel_format_desc);
    RNDR_RETURN_ON_FAIL(status != 0, ErrorCode::PlatformError, "Failed to set RGBA pixel format to the device context!", Destroy());

    HGLRC graphics_context = wglCreateContext(m_native_device_context);
    RNDR_RETURN_ON_FAIL(graphics_context != nullptr, ErrorCode::GraphicsAPIError, "Failed to create OpenGL graphics context!", Destroy());
    m_native_graphics_context = graphics_context;

    status = wglMakeCurrent(m_native_device_context, graphics_context);
    RNDR_RETURN_ON_FAIL(status != 0, ErrorCode::GraphicsAPIError, "Failed to make OpenGL graphics context current!", Destroy());

    status = gladLoadWGL(m_native_device_context);
    RNDR_RETURN_ON_FAIL(status != 0, ErrorCode::PlatformError, "Failed to load WGL functions!", Destroy());

    int arb_flags = WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
#if RNDR_DEBUG
    if (desc.enable_debug_layer)
    {
        arb_flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
    }
#endif
    const Opal::InPlaceArray<int, 9> attribute_list = {
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
    graphics_context = wglCreateContextAttribsARB(m_native_device_context, graphics_context, attribute_list.GetData());
    RNDR_RETURN_ON_FAIL(graphics_context != nullptr, ErrorCode::GraphicsAPIError, "Failed to create graphics context for OpenGL 4.6!",
                        Destroy());

    status = wglMakeCurrent(m_native_device_context, graphics_context);
    RNDR_RETURN_ON_FAIL(status != 0, ErrorCode::GraphicsAPIError, "Failed to make OpenGL graphics context current!", Destroy());

    status = wglDeleteContext(old_graphics_context);
    RNDR_RETURN_ON_FAIL(status != 0, ErrorCode::GraphicsAPIError, "Failed to delete temporary graphics context!", Destroy());

    status = gladLoadGL();
    RNDR_RETURN_ON_FAIL(status != 0, ErrorCode::PlatformError, "Failed to load OpenGL functions!", Destroy());

    m_native_graphics_context = graphics_context;

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&DebugOutputCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    RNDR_ASSERT_OPENGL();

    // Check if the hardware supports the required extensions.
    RNDR_RETURN_ON_FAIL(GLAD_GL_ARB_buffer_storage != 0, ErrorCode::FeatureNotSupported, "ARB_buffer_storage is not supported!", Destroy());
    RNDR_RETURN_ON_FAIL(GLAD_GL_ARB_direct_state_access != 0, ErrorCode::FeatureNotSupported, "ARB_direct_storage_access is not supported!",
                        Destroy());
    RNDR_RETURN_ON_FAIL(GLAD_GL_ARB_enhanced_layouts != 0, ErrorCode::FeatureNotSupported, "ARB_enhanced_layouts is not supported!",
                        Destroy());
    RNDR_RETURN_ON_FAIL(GLAD_GL_ARB_gl_spirv != 0, ErrorCode::FeatureNotSupported, "ARB_gl_spirv is not supported!", Destroy());
    RNDR_RETURN_ON_FAIL(GLAD_GL_ARB_indirect_parameters != 0, ErrorCode::FeatureNotSupported, "ARB_indirect_parameters is not supported!",
                        Destroy());
    RNDR_RETURN_ON_FAIL(GLAD_GL_ARB_multi_draw_indirect != 0, ErrorCode::FeatureNotSupported, "ARB_multi_draw_indirect is not supported!",
                        Destroy());
    RNDR_RETURN_ON_FAIL(GLAD_GL_ARB_shader_draw_parameters != 0, ErrorCode::FeatureNotSupported,
                        "ARB_shader_draw_parameters is not supported!", Destroy());
    RNDR_RETURN_ON_FAIL(GLAD_GL_ARB_texture_storage != 0, ErrorCode::FeatureNotSupported, "ARB_texture_storage is not supported!",
                        Destroy());
    RNDR_RETURN_ON_FAIL(!desc.enable_bindless_textures || GLAD_GL_ARB_gpu_shader_int64 != 0, ErrorCode::FeatureNotSupported,
                        "ARB_gpu_shader_int64 is not supported!", Destroy());
    RNDR_RETURN_ON_FAIL(!desc.enable_bindless_textures || GLAD_GL_ARB_bindless_texture != 0, ErrorCode::FeatureNotSupported,
                        "ARB_bindless_texture is not supported!", Destroy());

    m_desc = desc;
    return ErrorCode::Success;
#else
    RNDR_ASSERT(false, "Platform not supported");
    return ErrorCode::PlatformError;
#endif  // RNDR_WINDOWS
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
        else
        {
            m_native_graphics_context = k_invalid_graphics_context_handle;
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

void Rndr::GraphicsContext::ClearColor(const Vector4f& color)
{
    RNDR_CPU_EVENT_SCOPED("GraphicsContext::ClearColor");
    RNDR_GPU_EVENT_SCOPED("GraphicsContext::ClearColor");
    glClearColor(color.x, color.y, color.z, color.w);
    RNDR_GL_THROW_ON_ERROR("Failed to set clear color!", RNDR_NOOP);
    glClear(GL_COLOR_BUFFER_BIT);
    RNDR_GL_THROW_ON_ERROR("Failed to clear color buffer!", RNDR_NOOP);
}

void Rndr::GraphicsContext::ClearDepth(float depth)
{
    RNDR_CPU_EVENT_SCOPED("GraphicsContext::ClearDepth");
    RNDR_GPU_EVENT_SCOPED("GraphicsContext::ClearDepth");
    glClearDepth(depth);
    RNDR_GL_THROW_ON_ERROR("Failed to set clear depth!", RNDR_NOOP);
    glClear(GL_DEPTH_BUFFER_BIT);
    RNDR_GL_THROW_ON_ERROR("Failed to clear depth buffer!", RNDR_NOOP);
}

void Rndr::GraphicsContext::ClearStencil(i32 stencil)
{
    RNDR_CPU_EVENT_SCOPED("GraphicsContext::ClearStencil");
    RNDR_GPU_EVENT_SCOPED("GraphicsContext::ClearStencil");
    glClearStencil(stencil);
    RNDR_GL_THROW_ON_ERROR("Failed to set clear stencil!", RNDR_NOOP);
    glClear(GL_STENCIL_BUFFER_BIT);
    RNDR_GL_THROW_ON_ERROR("Failed to clear stencil buffer!", RNDR_NOOP);
}

void Rndr::GraphicsContext::ClearAll(const Vector4f& color, float depth, i32 stencil)
{
    RNDR_CPU_EVENT_SCOPED("GraphicsContext::ClearAll");
    RNDR_GPU_EVENT_SCOPED("GraphicsContext::ClearAll");
    glClearColor(color.x, color.y, color.z, color.w);
    RNDR_GL_THROW_ON_ERROR("Failed to set clear color!", RNDR_NOOP);
    glClearDepth(depth);
    RNDR_GL_THROW_ON_ERROR("Failed to set clear depth!", RNDR_NOOP);
    glClearStencil(stencil);
    RNDR_GL_THROW_ON_ERROR("Failed to set clear stencil value!", RNDR_NOOP);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    RNDR_GL_THROW_ON_ERROR("Failed to set clear color, depth and stencil buffers!", RNDR_NOOP);
}

void Rndr::GraphicsContext::BindPipeline(const Pipeline& pipeline)
{
    RNDR_CPU_EVENT_SCOPED("Bind Pipeline");
    RNDR_GPU_EVENT_SCOPED("Bind Pipeline");
    if (!pipeline.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Pipeline object is not valid");
    }

    const GLuint shader_program = pipeline.GetNativeShaderProgram();
    glUseProgram(shader_program);
    RNDR_GL_THROW_ON_ERROR("Failed to bind shader program!", RNDR_NOOP);
    const GLuint vertex_array = pipeline.GetNativeVertexArray();
    glBindVertexArray(vertex_array);
    RNDR_GL_THROW_ON_ERROR("Failed to bind vertex array object!", RNDR_NOOP);
    const Rndr::PipelineDesc desc = pipeline.GetDesc();
    if (desc.depth_stencil.is_depth_enabled)
    {
        glEnable(GL_DEPTH_TEST);
        RNDR_GL_THROW_ON_ERROR("Failed to enable depth test!", RNDR_NOOP);
        const GLenum depth_func = FromComparatorToOpenGL(desc.depth_stencil.depth_comparator);
        glDepthFunc(depth_func);
        RNDR_GL_THROW_ON_ERROR("Failed to set depth function!", RNDR_NOOP);
        glDepthMask(desc.depth_stencil.depth_mask == Rndr::DepthMask::All ? GL_TRUE : GL_FALSE);
        RNDR_GL_THROW_ON_ERROR("Failed to set depth mask!", RNDR_NOOP);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
        RNDR_GL_THROW_ON_ERROR("Failed to disable depth test!", RNDR_NOOP);
    }
    if (desc.depth_stencil.is_stencil_enabled)
    {
        constexpr u32 k_mask_all_enabled = 0xFFFFFFFF;
        glEnable(GL_STENCIL_TEST);
        RNDR_GL_THROW_ON_ERROR("Failed to enable stencil test!", RNDR_NOOP);
        glStencilMask(desc.depth_stencil.stencil_write_mask);
        RNDR_GL_THROW_ON_ERROR("Failed to set stencil write mask!", RNDR_NOOP);
        const GLenum front_face_stencil_func = FromComparatorToOpenGL(desc.depth_stencil.stencil_front_face_comparator);
        glStencilFuncSeparate(GL_FRONT, front_face_stencil_func, desc.depth_stencil.stencil_ref_value, k_mask_all_enabled);
        RNDR_GL_THROW_ON_ERROR("Failed to set front face stencil function!", RNDR_NOOP);
        const GLenum back_face_stencil_func = FromComparatorToOpenGL(desc.depth_stencil.stencil_back_face_comparator);
        glStencilFuncSeparate(GL_BACK, back_face_stencil_func, desc.depth_stencil.stencil_ref_value, k_mask_all_enabled);
        RNDR_GL_THROW_ON_ERROR("Failed to set back face stencil function!", RNDR_NOOP);
        const GLenum front_face_stencil_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_fail_op);
        const GLenum front_face_stencil_depth_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_depth_fail_op);
        const GLenum front_face_stencil_pass_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_front_face_pass_op);
        glStencilOpSeparate(GL_FRONT, front_face_stencil_fail_op, front_face_stencil_depth_fail_op, front_face_stencil_pass_op);
        RNDR_GL_THROW_ON_ERROR("Failed to set front face stencil operations!", RNDR_NOOP);
        const GLenum back_face_stencil_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_fail_op);
        const GLenum back_face_stencil_depth_fail_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_depth_fail_op);
        const GLenum back_face_stencil_pass_op = FromStencilOpToOpenGL(desc.depth_stencil.stencil_back_face_pass_op);
        glStencilOpSeparate(GL_BACK, back_face_stencil_fail_op, back_face_stencil_depth_fail_op, back_face_stencil_pass_op);
        RNDR_GL_THROW_ON_ERROR("Failed to set back face stencil operations!", RNDR_NOOP);
    }
    else
    {
        glDisable(GL_STENCIL_TEST);
        RNDR_GL_THROW_ON_ERROR("Failed to disable stencil test!", RNDR_NOOP);
    }
    if (desc.blend.is_enabled)
    {
        glEnable(GL_BLEND);
        RNDR_GL_THROW_ON_ERROR("Failed to enable blending!", RNDR_NOOP);
        const GLenum src_color_factor = FromBlendFactorToOpenGL(desc.blend.src_color_factor);
        const GLenum dst_color_factor = FromBlendFactorToOpenGL(desc.blend.dst_color_factor);
        const GLenum src_alpha_factor = FromBlendFactorToOpenGL(desc.blend.src_alpha_factor);
        const GLenum dst_alpha_factor = FromBlendFactorToOpenGL(desc.blend.dst_alpha_factor);
        const GLenum color_op = FromBlendOperationToOpenGL(desc.blend.color_operation);
        const GLenum alpha_op = FromBlendOperationToOpenGL(desc.blend.alpha_operation);
        glBlendFuncSeparate(src_color_factor, dst_color_factor, src_alpha_factor, dst_alpha_factor);
        RNDR_GL_THROW_ON_ERROR("Failed to set blend factors!", RNDR_NOOP);
        glBlendEquationSeparate(color_op, alpha_op);
        RNDR_GL_THROW_ON_ERROR("Failed to set blend operations!", RNDR_NOOP);
        glBlendColor(desc.blend.const_color.r, desc.blend.const_color.g, desc.blend.const_color.b, desc.blend.const_alpha);
        RNDR_GL_THROW_ON_ERROR("Failed to set blend constant color!", RNDR_NOOP);
    }
    else
    {
        glDisable(GL_BLEND);
        RNDR_GL_THROW_ON_ERROR("Failed to disable blending!", RNDR_NOOP);
    }
    // Rasterizer configuration
    {
        glPolygonMode(GL_FRONT_AND_BACK, desc.rasterizer.fill_mode == FillMode::Solid ? GL_FILL : GL_LINE);
        RNDR_GL_THROW_ON_ERROR("Failed to set polygon mode!", RNDR_NOOP);
        if (desc.rasterizer.cull_face != Face::None)
        {
            glEnable(GL_CULL_FACE);
            RNDR_GL_THROW_ON_ERROR("Failed to enable cull face!", RNDR_NOOP);
            glCullFace(desc.rasterizer.cull_face == Face::Front ? GL_FRONT : GL_BACK);
            RNDR_GL_THROW_ON_ERROR("Failed to set cull face!", RNDR_NOOP);
        }
        else
        {
            glDisable(GL_CULL_FACE);
            RNDR_GL_THROW_ON_ERROR("Failed to disable cull face!", RNDR_NOOP);
        }
        glFrontFace(desc.rasterizer.front_face_winding_order == WindingOrder::CW ? GL_CW : GL_CCW);
        RNDR_GL_THROW_ON_ERROR("Failed to set front face winding order!", RNDR_NOOP);
        if (desc.rasterizer.depth_bias != 0.0f || desc.rasterizer.slope_scaled_depth_bias != 0.0f)
        {
            glEnable(GL_POLYGON_OFFSET_LINE);
            RNDR_GL_THROW_ON_ERROR("Failed to enable polygon offset!", RNDR_NOOP);
            glPolygonOffset(desc.rasterizer.slope_scaled_depth_bias, desc.rasterizer.depth_bias);
            RNDR_GL_THROW_ON_ERROR("Failed to set polygon offset!", RNDR_NOOP);
        }
        else
        {
            glDisable(GL_POLYGON_OFFSET_LINE);
            RNDR_GL_THROW_ON_ERROR("Failed to disable polygon offset!", RNDR_NOOP);
        }
        if (desc.rasterizer.scissor_size.x > 0 && desc.rasterizer.scissor_size.y > 0)
        {
            glEnable(GL_SCISSOR_TEST);
            RNDR_GL_THROW_ON_ERROR("Failed to enable scissor test!", RNDR_NOOP);
            glScissor(static_cast<i32>(desc.rasterizer.scissor_bottom_left.x), static_cast<i32>(desc.rasterizer.scissor_bottom_left.y),
                      static_cast<i32>(desc.rasterizer.scissor_size.x), static_cast<i32>(desc.rasterizer.scissor_size.y));
            RNDR_GL_THROW_ON_ERROR("Failed to set scissor rectangle!", RNDR_NOOP);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
            RNDR_GL_THROW_ON_ERROR("Failed to disable scissor test!", RNDR_NOOP);
        }
    }
    m_bound_pipeline = &pipeline;
}

void Rndr::GraphicsContext::BindBuffer(const Buffer& buffer, i32 binding_index)
{
    RNDR_CPU_EVENT_SCOPED("Bind Buffer");

    if (!buffer.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Buffer object is invalid");
    }
    const GLuint native_buffer = buffer.GetNativeBuffer();
    const BufferDesc& desc = buffer.GetDesc();
    const GLenum target = FromBufferTypeToOpenGL(desc.type);
    if (target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER)
    {
        glBindBufferRange(target, binding_index, native_buffer, 0, desc.size);
        RNDR_GL_THROW_ON_ERROR("Failed to bind a buffer!", RNDR_NOOP);
        return;
    }
    throw Opal::InvalidArgumentException(__FUNCTION__,
                                         "Vertex and index buffers should be bound through the use of input layout and pipeline");
}

void Rndr::GraphicsContext::BindTexture(const Texture& texture, i32 binding_index)
{
    RNDR_CPU_EVENT_SCOPED("Bind Texture");

    if (!texture.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Texture object is invalid");
    }
    const GLuint native_texture = texture.GetNativeTexture();
    glBindTextures(binding_index, 1, &native_texture);
    RNDR_GL_THROW_ON_ERROR("Failed to bind texture!", RNDR_NOOP);
}

void Rndr::GraphicsContext::BindTextureForCompute(const Rndr::Texture& texture, i32 binding_index, i32 image_level,
                                                  Rndr::TextureAccess access)
{
    RNDR_CPU_EVENT_SCOPED("Bind Texture For Compute");

    if (!texture.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Texture object is invalid");
    }
    glBindImageTexture(binding_index, texture.GetNativeTexture(), image_level, GL_FALSE, 0, FromImageAccessToOpenGL(access),
                       FromPixelFormatToInternalFormat(texture.GetTextureDesc().pixel_format));
    RNDR_GL_THROW_ON_ERROR("Failed to bind texture for compute!", RNDR_NOOP);
}

void Rndr::GraphicsContext::BindFrameBuffer(const Rndr::FrameBuffer& frame_buffer)
{
    RNDR_CPU_EVENT_SCOPED("Bind FrameBuffer");

    if (!frame_buffer.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Frame buffer object is invalid");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer.GetNativeFrameBuffer());
    RNDR_GL_THROW_ON_ERROR("Failed to bind frame buffer!", RNDR_NOOP);
    TextureDesc attachment_desc;
    if (frame_buffer.GetColorAttachmentCount() > 0)
    {
        attachment_desc = frame_buffer.GetColorAttachment(0).GetTextureDesc();
    }
    else
    {
        attachment_desc = frame_buffer.GetDepthStencilAttachment().GetTextureDesc();
    }
    glViewport(0, 0, attachment_desc.width, attachment_desc.height);
    RNDR_GL_THROW_ON_ERROR("Failed to set viewport!", RNDR_NOOP);
}

void Rndr::GraphicsContext::BindSwapChainFrameBuffer(const Rndr::SwapChain& swap_chain)
{
    RNDR_CPU_EVENT_SCOPED("Bind SwapChain FrameBuffer");

    if (!swap_chain.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Swap chain object is invalid");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    RNDR_GL_THROW_ON_ERROR("Failed to bind default frame buffer!", RNDR_NOOP);
    const SwapChainDesc& desc = swap_chain.GetDesc();
    glViewport(0, 0, desc.width, desc.height);
    RNDR_GL_THROW_ON_ERROR("Failed to set viewport!", RNDR_NOOP);
}

void Rndr::GraphicsContext::DrawVertices(PrimitiveTopology topology, i32 vertex_count, i32 instance_count, i32 first_vertex)
{
    RNDR_CPU_EVENT_SCOPED("Draw Vertices");
    RNDR_GPU_EVENT_SCOPED("Draw Vertices");

    const GLenum primitive = FromPrimitiveTopologyToOpenGL(topology);
    glDrawArraysInstanced(primitive, first_vertex, vertex_count, instance_count);
    RNDR_GL_THROW_ON_ERROR("DrawVertices failed!", RNDR_NOOP);
}

void Rndr::GraphicsContext::DrawIndices(PrimitiveTopology topology, i32 index_count, i32 instance_count, i32 first_index)
{
    RNDR_CPU_EVENT_SCOPED("Draw Indices");
    RNDR_GPU_EVENT_SCOPED("Draw Indices");

    RNDR_ASSERT(m_bound_pipeline.IsValid(), "No pipeline is bound!");
    RNDR_ASSERT(m_bound_pipeline->IsIndexBufferBound(), "No index buffer is bound!");

    const i64 index_size = m_bound_pipeline->GetIndexBufferElementSize();
    const GLenum index_size_enum = FromIndexSizeToOpenGL(index_size);
    const i64 index_offset = index_size * first_index;
    void* index_start = reinterpret_cast<void*>(index_offset);
    const GLenum primitive = FromPrimitiveTopologyToOpenGL(topology);
    glDrawElementsInstanced(primitive, index_count, index_size_enum, index_start, instance_count);
    RNDR_GL_THROW_ON_ERROR("DrawIndices failed!", RNDR_NOOP);
}

void Rndr::GraphicsContext::DispatchCompute(u32 block_count_x, u32 block_count_y, u32 block_count_z, bool wait_for_completion)
{
    RNDR_CPU_EVENT_SCOPED("Dispatch Compute");

    glDispatchCompute(block_count_x, block_count_y, block_count_z);
    RNDR_GL_THROW_ON_ERROR("Dispatch of compute work failed!", RNDR_NOOP);
    if (wait_for_completion)
    {
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        RNDR_GL_THROW_ON_ERROR("Memory barrier failed!", RNDR_NOOP);
    }
}

void Rndr::GraphicsContext::UpdateBuffer(const Buffer& buffer, const Opal::ArrayView<const u8>& data, i64 offset)
{
    RNDR_CPU_EVENT_SCOPED("Update Buffer Contents");
    RNDR_GPU_EVENT_SCOPED("Update Buffer Contents");

    if (!buffer.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Buffer object is invalid");
    }
    const BufferDesc desc = buffer.GetDesc();
    if (desc.usage != Usage::Dynamic)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Buffer usage is not dynamic");
    }
    const i64 buffer_size = static_cast<i64>(desc.size);
    if (offset < 0 || offset >= buffer_size)
    {
        throw Opal::OutOfBoundsException(static_cast<u64>(offset), 0, desc.size);
    }
    const i64 data_size = static_cast<i64>(data.GetSize());
    if (data_size < 0 || offset + data_size > buffer_size)
    {
        throw Opal::OutOfBoundsException(static_cast<u64>(offset + data_size), 0, desc.size);
    }
    if (data_size == 0)
    {
        return;
    }

    const GLuint native_buffer = buffer.GetNativeBuffer();
    glNamedBufferSubData(native_buffer, offset, data_size, data.GetData());
    RNDR_GL_THROW_ON_ERROR("Buffer update failed", RNDR_NOOP);
}

void Rndr::GraphicsContext::ReadBuffer(const Buffer& buffer, Opal::ArrayView<u8>& out_data, i32 offset, i32 size) const
{
    RNDR_CPU_EVENT_SCOPED("Read Buffer Contents");
    RNDR_GPU_EVENT_SCOPED("Read Buffer Contents");

    if (!buffer.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Buffer object is invalid");
    }
    const BufferDesc& desc = buffer.GetDesc();
    if (desc.usage != Usage::ReadBack)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Buffer usage is not readback");
    }

    // If size is 0, read to the end of the buffer
    if (size == 0)
    {
        size = static_cast<i32>(desc.size) - offset;
    }
    if (offset < 0 || offset >= static_cast<i32>(desc.size))
    {
        throw Opal::OutOfBoundsException(static_cast<u64>(offset), 0, desc.size);
    }
    if (offset + size > static_cast<i32>(desc.size))
    {
        throw Opal::OutOfBoundsException(static_cast<u64>(offset + size), 0, desc.size);
    }

    const GLuint native_buffer = buffer.GetNativeBuffer();
    u8* gpu_data = static_cast<u8*>(glMapNamedBufferRange(native_buffer, desc.offset, static_cast<i32>(desc.size), GL_MAP_READ_BIT));
    RNDR_GL_THROW_ON_ERROR("Mapping of the buffer contents failed", RNDR_NOOP);

    RNDR_ASSERT(gpu_data != nullptr, "Mapping of GPU memory failed!");
    memcpy(out_data.GetData(), gpu_data + offset, size);

    glUnmapNamedBuffer(native_buffer);
    RNDR_GL_THROW_ON_ERROR("Unmapping of the buffer contents failed", RNDR_NOOP);
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

void Rndr::GraphicsContext::CopyBuffer(const Buffer& dst_buffer, const Buffer& src_buffer, i32 dst_offset, i32 src_offset, i32 size)
{
    RNDR_CPU_EVENT_SCOPED("Copy Buffer Contents");
    RNDR_GPU_EVENT_SCOPED("Copy Buffer Contents");

    if (!dst_buffer.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Destination buffer object is invalid");
    }
    if (!src_buffer.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Source buffer object is invalid");
    }

    const BufferDesc& dst_desc = dst_buffer.GetDesc();
    const BufferDesc& src_desc = src_buffer.GetDesc();

    if (src_offset < 0 || src_offset >= static_cast<i32>(src_desc.size))
    {
        throw Opal::OutOfBoundsException(static_cast<u64>(src_offset), 0, src_desc.size);
    }
    if (dst_offset < 0 || dst_offset >= static_cast<i32>(dst_desc.size))
    {
        throw Opal::OutOfBoundsException(static_cast<u64>(dst_offset), 0, dst_desc.size);
    }

    if (size == 0)
    {
        const i32 src_remaining_size = static_cast<i32>(src_desc.size) - src_offset;
        const i32 dst_remaining_size = static_cast<i32>(dst_desc.size) - dst_offset;
        size = Opal::Min(src_remaining_size, dst_remaining_size);
    }
    if (size == 0)
    {
        return;
    }

    if (dst_offset + size > static_cast<i32>(dst_desc.size))
    {
        throw Opal::OutOfBoundsException(static_cast<u64>(dst_offset + size), 0, dst_desc.size);
    }
    if (src_offset + size > static_cast<i32>(src_desc.size))
    {
        throw Opal::OutOfBoundsException(static_cast<u64>(src_offset + size), 0, src_desc.size);
    }
    if (src_buffer.GetNativeBuffer() == dst_buffer.GetNativeBuffer() && Overlap(src_offset, dst_offset, size))
    {
        throw Opal::InvalidArgumentException(__FUNCTION__,
                                             "Source and destination point to the same buffer object and their ranges overlap");
    }

    glCopyNamedBufferSubData(src_buffer.GetNativeBuffer(), dst_buffer.GetNativeBuffer(), src_offset, dst_offset, size);
    RNDR_GL_THROW_ON_ERROR("Failed to copy contents from one buffer to another", RNDR_NOOP);
}

void Rndr::GraphicsContext::Read(const Texture& image, Bitmap& out_data, i32 level) const
{
    RNDR_CPU_EVENT_SCOPED("Read Texture Contents");
    RNDR_GPU_EVENT_SCOPED("Read Texture Contents");

    if (!image.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Texture object is invalid");
    }

    const TextureDesc& desc = image.GetTextureDesc();
    const GLenum format = FromPixelFormatToExternalFormat(desc.pixel_format);
    const GLenum data_type = FromPixelFormatToDataType(desc.pixel_format);
    const i32 pixel_size = FromPixelFormatToPixelSize(desc.pixel_format);
    Opal::DynamicArray<u8> tmp_data(pixel_size * desc.width * desc.height);
    glGetTextureImage(image.GetNativeTexture(), level, format, data_type, pixel_size * desc.width * desc.height, tmp_data.GetData());
    RNDR_GL_THROW_ON_ERROR("Failed to get texture image contents", RNDR_NOOP);

    out_data = Bitmap(desc.width, desc.height, 1, desc.pixel_format, 1, AsWritableBytes(tmp_data));
}

void Rndr::GraphicsContext::ReadSwapChainColor(const SwapChain& swap_chain, Bitmap& out_bitmap)
{
    RNDR_CPU_EVENT_SCOPED("Read SwapChain");

    const i32 width = swap_chain.GetDesc().width;
    const i32 height = swap_chain.GetDesc().height;
    const i32 size = width * height * 4;
    Opal::DynamicArray<u8> data(size);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.GetData());
    RNDR_GL_THROW_ON_ERROR("Failed to read pixel value from the swap chain", RNDR_NOOP);
    for (i32 i = 0; i < height / 2; i++)
    {
        for (i32 j = 0; j < width; j++)
        {
            const i32 index1 = i * width * 4 + j * 4;
            const i32 index2 = (height - i - 1) * width * 4 + j * 4;
            Opal::Swap(data[index1], data[index2]);
            Opal::Swap(data[index1 + 1], data[index2 + 1]);
            Opal::Swap(data[index1 + 2], data[index2 + 2]);
            Opal::Swap(data[index1 + 3], data[index2 + 3]);
        }
    }
    out_bitmap = Bitmap{width, height, 1, PixelFormat::R8G8B8A8_SRGB, 1,AsWritableBytes(data)};
}

void Rndr::GraphicsContext::ReadSwapChainDepthStencil(const SwapChain& swap_chain, Bitmap& out_bitmap)
{
    RNDR_CPU_EVENT_SCOPED("Read SwapChain");

    const i32 width = swap_chain.GetDesc().width;
    const i32 height = swap_chain.GetDesc().height;
    const i32 size = width * height;
    Opal::DynamicArray<u32> data(size);
    glReadPixels(0, 0, width, height, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, data.GetData());
    RNDR_GL_THROW_ON_ERROR("Failed to read depth and stencil values from the swap chain", RNDR_NOOP)
    const Opal::ArrayView<u8> byte_data(reinterpret_cast<u8*>(data.GetData()), size * sizeof(u32));
    out_bitmap = Bitmap{width, height, 1, PixelFormat::D24_UNORM_S8_UINT, 1, byte_data};
}

void Rndr::GraphicsContext::ClearFrameBufferColorAttachment(const FrameBuffer& frame_buffer, i32 color_attachment_index,
                                                            const Vector4f& color)
{
    if (!frame_buffer.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Frame buffer object is invalid");
    }
    if (color_attachment_index < 0 || color_attachment_index >= frame_buffer.GetColorAttachmentCount())
    {
        throw Opal::OutOfBoundsException(static_cast<i64>(color_attachment_index), 0,
                                         static_cast<i64>(frame_buffer.GetColorAttachmentCount() - 1));
    }
    glClearNamedFramebufferfv(frame_buffer.GetNativeFrameBuffer(), GL_COLOR, color_attachment_index, color.data);
    RNDR_GL_THROW_ON_ERROR("Failed to clear color attachment!", RNDR_NOOP);
}

void Rndr::GraphicsContext::ClearFrameBufferDepthStencilAttachment(const Rndr::FrameBuffer& frame_buffer, f32 depth, i32 stencil)
{
    if (!frame_buffer.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Frame buffer object is invalid");
    }
    glClearNamedFramebufferfi(frame_buffer.GetNativeFrameBuffer(), GL_DEPTH_STENCIL, 0, depth, stencil);
    RNDR_GL_THROW_ON_ERROR("Failed to clear depth attachment!", RNDR_NOOP);
}

void Rndr::GraphicsContext::BlitFrameBuffers(const FrameBuffer& dst, const FrameBuffer& src, const BlitFrameBufferDesc& desc)
{
    RNDR_CPU_EVENT_SCOPED("Blit Frame Buffers");
    RNDR_GPU_EVENT_SCOPED("Blit Frame Buffers");

    if (!dst.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Destination frame buffer object is invalid");
    }
    if (!src.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Source frame buffer object is invalid");
    }
    if ((desc.should_copy_depth || desc.should_copy_stencil) && desc.interpolation != ImageFilter::Nearest)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Depth and stencil attachments can only be copied with nearest interpolation");
    }

    Vector2i src_size = desc.src_size;
    if (src_size.x == 0)
    {
        src_size.x = src.GetDesc().color_attachments[0].width;
    }
    if (src_size.y == 0)
    {
        src_size.y = src.GetDesc().color_attachments[0].height;
    }
    Vector2i dst_size = desc.dst_size;
    if (dst_size.x == 0)
    {
        dst_size.x = dst.GetDesc().color_attachments[0].width;
    }
    if (dst_size.y == 0)
    {
        dst_size.y = dst.GetDesc().color_attachments[0].height;
    }
    GLuint mask = 0;
    if (desc.should_copy_color)
    {
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (desc.should_copy_depth)
    {
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    if (desc.should_copy_stencil)
    {
        mask |= GL_STENCIL_BUFFER_BIT;
    }
    const GLuint filter = FromImageFilterToOpenGL(desc.interpolation);
    glBlitNamedFramebuffer(src.GetNativeFrameBuffer(), dst.GetNativeFrameBuffer(), desc.src_offset.x, desc.src_offset.y, src_size.x,
                           src_size.y, desc.dst_offset.x, desc.dst_offset.y, dst_size.x, dst_size.y, mask, filter);
    RNDR_GL_THROW_ON_ERROR("Failed to blit frame buffer into another frame buffer!", RNDR_NOOP);
}

void Rndr::GraphicsContext::BlitToSwapChain(const SwapChain& swap_chain, const FrameBuffer& src, const BlitFrameBufferDesc& desc)
{
    RNDR_CPU_EVENT_SCOPED("Blit To SwapChain");
    RNDR_GPU_EVENT_SCOPED("Blit To SwapChain");

    if (!swap_chain.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Swap chain object is invalid");
    }
    if (!src.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Source frame buffer object is invalid");
    }
    if ((desc.should_copy_depth || desc.should_copy_stencil) && desc.interpolation != ImageFilter::Nearest)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Depth and stencil attachments can only be copied with nearest interpolation");
    }

    Vector2i src_size = desc.src_size;
    if (src_size.x == 0)
    {
        src_size.x = src.GetDesc().color_attachments[0].width;
    }
    if (src_size.y == 0)
    {
        src_size.y = src.GetDesc().color_attachments[0].height;
    }
    Vector2i dst_size = desc.dst_size;
    if (dst_size.x == 0)
    {
        dst_size.x = swap_chain.GetDesc().width;
    }
    if (dst_size.y == 0)
    {
        dst_size.y = swap_chain.GetDesc().height;
    }
    GLuint mask = 0;
    if (desc.should_copy_color)
    {
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (desc.should_copy_depth)
    {
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    if (desc.should_copy_stencil)
    {
        mask |= GL_STENCIL_BUFFER_BIT;
    }
    const GLuint filter = FromImageFilterToOpenGL(desc.interpolation);
    glBlitNamedFramebuffer(src.GetNativeFrameBuffer(), 0, desc.src_offset.x, desc.src_offset.y, src_size.x, src_size.y, desc.dst_offset.x,
                           desc.dst_offset.y, dst_size.x, dst_size.y, mask, filter);
    RNDR_GL_THROW_ON_ERROR("Failed to blit frame buffer into a swap chain!", RNDR_NOOP);
}

void Rndr::GraphicsContext::SubmitCommandList(CommandList& command_list)
{
    command_list.Execute();
}
