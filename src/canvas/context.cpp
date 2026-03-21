#include "rndr/canvas/context.hpp"

#include "glad/glad_wgl.h"

#include "opal/container/hash-set.h"

#include "rndr/definitions.hpp"
#include "rndr/exception.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/log.hpp"
#include "rndr/trace.hpp"

namespace
{

Opal::HashSet<size_t> g_log_messages;
void APIENTRY DebugOutputCallback(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message,
                                  const void* user_param)
{
    RNDR_UNUSED(source);
    RNDR_UNUSED(type);
    RNDR_UNUSED(id);
    RNDR_UNUSED(length);
    RNDR_UNUSED(user_param);
    if (type == GL_DEBUG_TYPE_PUSH_GROUP || type == GL_DEBUG_TYPE_POP_GROUP)
    {
        return;
    }
    const size_t message_key = Opal::Hash::CalcPOD(id) ^ (Opal::Hash::CalcPOD(source) << 1) ^ (Opal::Hash::CalcPOD(type) << 2);
    if (g_log_messages.Contains(message_key))
    {
        return;
    }
    g_log_messages.Insert(message_key);
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:
            RNDR_LOG_ERROR("{}", message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            RNDR_LOG_WARNING("{}", message);
            break;
        case GL_DEBUG_SEVERITY_LOW:
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            RNDR_LOG_INFO("{}", message);
            break;
        default:
            break;
    }
}

/**
 * Map a Canvas color format to the number of color bits and alpha bits for the pixel format
 * descriptor.
 */
void GetColorBits(Rndr::Canvas::Format format, BYTE& out_color_bits, BYTE& out_alpha_bits)
{
    switch (format)
    {
        case Rndr::Canvas::Format::RGB8:
        case Rndr::Canvas::Format::SRGB8:
            out_color_bits = 24;
            out_alpha_bits = 0;
            break;
        case Rndr::Canvas::Format::RGBA8:
        case Rndr::Canvas::Format::SRGBA8:
        default:
            out_color_bits = 32;
            out_alpha_bits = 8;
            break;
    }
}

/**
 * Map a Canvas depth/stencil format to the number of depth bits and stencil bits for the pixel
 * format descriptor.
 */
void GetDepthStencilBits(Rndr::Canvas::Format format, BYTE& out_depth_bits, BYTE& out_stencil_bits)
{
    switch (format)
    {
        case Rndr::Canvas::Format::D32F:
            out_depth_bits = 32;
            out_stencil_bits = 0;
            break;
        case Rndr::Canvas::Format::D24S8:
        default:
            out_depth_bits = 24;
            out_stencil_bits = 8;
            break;
    }
}

}  // namespace

Rndr::Canvas::Context::Context() = default;

Rndr::Canvas::Context::~Context()
{
    Destroy();
}

Rndr::Canvas::Context::Context(Context&& other) noexcept
    : m_vsync_enabled(other.m_vsync_enabled),
      m_color_format(other.m_color_format),
      m_depth_stencil_format(other.m_depth_stencil_format),
      m_window(std::move(other.m_window)),
      m_device_context(other.m_device_context),
      m_graphics_context(other.m_graphics_context),
      m_width(other.m_width),
      m_height(other.m_height)
{
    other.m_window = nullptr;
    other.m_device_context = k_invalid_device_context_handle;
    other.m_graphics_context = k_invalid_graphics_context_handle;
    other.m_width = 0;
    other.m_height = 0;
}

Rndr::Canvas::Context& Rndr::Canvas::Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_vsync_enabled = other.m_vsync_enabled;
        m_color_format = other.m_color_format;
        m_depth_stencil_format = other.m_depth_stencil_format;
        m_window = std::move(other.m_window);
        m_device_context = other.m_device_context;
        m_graphics_context = other.m_graphics_context;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_window = nullptr;
        other.m_device_context = k_invalid_device_context_handle;
        other.m_graphics_context = k_invalid_graphics_context_handle;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void Rndr::Canvas::Context::Destroy()
{
#if RNDR_WINDOWS
    if (m_graphics_context != k_invalid_graphics_context_handle)
    {
        const BOOL status = wglDeleteContext(m_graphics_context);
        if (status == 0)
        {
            RNDR_LOG_ERROR("Failed to destroy OpenGL context!");
        }
        m_graphics_context = k_invalid_graphics_context_handle;
    }
    if (m_device_context != k_invalid_device_context_handle && m_window != nullptr)
    {
        ReleaseDC(RNDR_TO_HWND(m_window->GetNativeHandle()), m_device_context);
    }
    m_device_context = k_invalid_device_context_handle;
    m_window = nullptr;
    m_width = 0;
    m_height = 0;
    g_context_exists = false;
#endif
}

void Rndr::Canvas::Context::Present()
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Context::Present");

#if RNDR_WINDOWS
    if (m_device_context != k_invalid_device_context_handle)
    {
        SwapBuffers(m_device_context);
    }
#endif
}

void Rndr::Canvas::Context::SetVsync(bool enabled)
{
    m_vsync_enabled = enabled;
#if RNDR_WINDOWS
    wglSwapIntervalEXT(enabled ? 1 : 0);
#endif
}

void Rndr::Canvas::Context::Resize(i32 width, i32 height)
{
    m_width = width;
    m_height = height;
}

Rndr::i32 Rndr::Canvas::Context::GetWidth() const
{
    return m_width;
}

Rndr::i32 Rndr::Canvas::Context::GetHeight() const
{
    return m_height;
}

bool Rndr::Canvas::Context::IsVsyncEnabled() const
{
    return m_vsync_enabled;
}

Rndr::Canvas::Format Rndr::Canvas::Context::GetColorFormat() const
{
    return m_color_format;
}

Rndr::Canvas::Format Rndr::Canvas::Context::GetDepthStencilFormat() const
{
    return m_depth_stencil_format;
}

bool Rndr::Canvas::Context::IsValid() const
{
    return m_device_context != k_invalid_device_context_handle && m_graphics_context != k_invalid_graphics_context_handle;
}

#define RNDR_IS_EXTENSION_ALLOWED(extension)

bool Rndr::Canvas::Context::g_context_exists = false;
Rndr::Canvas::Context Rndr::Canvas::Context::Init(Opal::Ref<GenericWindow> window, const ContextDesc& desc)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Context::Init");

    if (g_context_exists)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Canvas::Context::Init called twice! Only one Context can exist at a time.");
    }

    Context ctx;
    ctx.m_vsync_enabled = desc.vsync_enabled;
    ctx.m_color_format = desc.color_format;
    ctx.m_depth_stencil_format = desc.depth_stencil_format;

#if RNDR_WINDOWS
    if (!window.IsValid())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Window handle is null!");
    }

    ctx.m_window = std::move(window);
    ctx.m_device_context = GetDC(RNDR_TO_HWND(ctx.m_window->GetNativeHandle()));
    if (ctx.m_device_context == k_invalid_device_context_handle)
    {
        throw GraphicsAPIException(0, "Failed to get device context from window!");
    }

    BYTE color_bits = 32;
    BYTE alpha_bits = 8;
    GetColorBits(desc.color_format, color_bits, alpha_bits);

    BYTE depth_bits = 24;
    BYTE stencil_bits = 8;
    GetDepthStencilBits(desc.depth_stencil_format, depth_bits, stencil_bits);

    PIXELFORMATDESCRIPTOR pixel_format_desc = {};
    pixel_format_desc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pixel_format_desc.nVersion = 1;
    pixel_format_desc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixel_format_desc.iPixelType = PFD_TYPE_RGBA;
    pixel_format_desc.cColorBits = color_bits;
    pixel_format_desc.cAlphaBits = alpha_bits;
    pixel_format_desc.cDepthBits = depth_bits;
    pixel_format_desc.cStencilBits = stencil_bits;

    const int pixel_format = ChoosePixelFormat(ctx.m_device_context, &pixel_format_desc);
    if (pixel_format == 0)
    {
        throw GraphicsAPIException(0, "Pixel format is not supported!");
    }

    BOOL status = SetPixelFormat(ctx.m_device_context, pixel_format, &pixel_format_desc);
    if (status == 0)
    {
        throw GraphicsAPIException(0, "Failed to set pixel format!");
    }

    // Create a temporary context to bootstrap WGL extensions.
    HGLRC temp_context = wglCreateContext(ctx.m_device_context);
    if (temp_context == nullptr)
    {
        throw GraphicsAPIException(0, "Failed to create temporary OpenGL context!");
    }

    status = wglMakeCurrent(ctx.m_device_context, temp_context);
    if (status == 0)
    {
        wglDeleteContext(temp_context);
        throw GraphicsAPIException(0, "Failed to make temporary context current!");
    }

    // Try to load WGL extensions and create a core profile context. This may fail on software
    // renderers (e.g. Mesa) that don't support WGL ARB extensions but already provide a full
    // OpenGL 4.6 context via the basic wglCreateContext path.
    HGLRC final_context = nullptr;
    if (gladLoadWGL(ctx.m_device_context) != 0 && wglCreateContextAttribsARB != nullptr)
    {
        int arb_flags = WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
#if RNDR_DEBUG
        arb_flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
#endif

        const Opal::InPlaceArray<i32, 9> attribute_list = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                                                           4,
                                                           WGL_CONTEXT_MINOR_VERSION_ARB,
                                                           6,
                                                           WGL_CONTEXT_FLAGS_ARB,
                                                           arb_flags,
                                                           WGL_CONTEXT_PROFILE_MASK_ARB,
                                                           WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                                                           0};

        HGLRC arb_context = wglCreateContextAttribsARB(ctx.m_device_context, nullptr, attribute_list.GetData());
        if (arb_context != nullptr)
        {
            status = wglMakeCurrent(ctx.m_device_context, arb_context);
            if (status != 0)
            {
                wglDeleteContext(temp_context);
                final_context = arb_context;
            }
            else
            {
                wglDeleteContext(arb_context);
            }
        }
    }

    // Fallback: use the basic context directly.
    if (final_context == nullptr)
    {
        wglDeleteContext(temp_context);
        throw GraphicsAPIException(0, "WGL ARB extensions not available!");
    }

    status = gladLoadGL();
    if (status == 0)
    {
        wglDeleteContext(final_context);
        throw GraphicsAPIException(0, "Failed to load OpenGL functions!");
    }

    ctx.m_graphics_context = final_context;

    // Query initial window dimensions.
    RECT client_rect = {};
    GetClientRect(RNDR_TO_HWND(ctx.m_window->GetNativeHandle()), &client_rect);
    ctx.m_width = static_cast<i32>(client_rect.right - client_rect.left);
    ctx.m_height = static_cast<i32>(client_rect.bottom - client_rect.top);

    // Set vsync.
    wglSwapIntervalEXT(ctx.m_vsync_enabled ? 1 : 0);

    // Enable SRGB framebuffer if an SRGB color format was requested.
    if (desc.color_format == Format::SRGB8 || desc.color_format == Format::SRGBA8)
    {
        glEnable(GL_FRAMEBUFFER_SRGB);
    }

#if RNDR_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&DebugOutputCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    // Ignore message that says buffer has usage hint GL_STATIC_DRAW, since this is old API not in use and makes no sense
    constexpr GLuint k_nvidia_buffer_info_bit = 131185;
    glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 1, &k_nvidia_buffer_info_bit, GL_FALSE);
#endif

#else
    RNDR_UNUSED(window);
    throw GraphicsAPIException(0, "Platform not supported!");
#endif

    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    g_context_exists = true;
    RNDR_LOG_INFO("OpenGL {}.{} context initialized successfully.", major, minor);
    return ctx;
}