#include "rndr/canvas/context.hpp"

#include <array>

#include "glad/glad_wgl.h"

#include "opal/logging.h"

#include "rndr/definitions.hpp"
#include "rndr/exception.hpp"
#include "rndr/trace.hpp"

namespace
{

constexpr const char* k_log_category = "Canvas";

void APIENTRY DebugOutputCallback(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message,
                                  const void* user_param)
{
    RNDR_UNUSED(source);
    RNDR_UNUSED(type);
    RNDR_UNUSED(id);
    RNDR_UNUSED(length);
    RNDR_UNUSED(user_param);
    Opal::Logger& logger = Opal::GetLogger();
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:
            logger.Error(k_log_category, "{}", message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            logger.Warning(k_log_category, "{}", message);
            break;
        default:
            break;
    }
}

}  // namespace

Rndr::Canvas::Context::Context() = default;

Rndr::Canvas::Context::~Context()
{
#if RNDR_WINDOWS
    if (m_graphics_context != k_invalid_graphics_context_handle)
    {
        const BOOL status = wglDeleteContext(m_graphics_context);
        if (status == 0)
        {
            Opal::GetLogger().Error(k_log_category, "Failed to destroy OpenGL context!");
        }
        m_graphics_context = k_invalid_graphics_context_handle;
        m_device_context = k_invalid_device_context_handle;
    }
    g_context_exists = false;
#endif
}

Rndr::Canvas::Context::Context(Context&& other) noexcept
    : m_device_context(other.m_device_context), m_graphics_context(other.m_graphics_context)
{
    other.m_device_context = k_invalid_device_context_handle;
    other.m_graphics_context = k_invalid_graphics_context_handle;
}

Rndr::Canvas::Context& Rndr::Canvas::Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        this->~Context();
        m_device_context = other.m_device_context;
        m_graphics_context = other.m_graphics_context;
        other.m_device_context = k_invalid_device_context_handle;
        other.m_graphics_context = k_invalid_graphics_context_handle;
    }
    return *this;
}

bool Rndr::Canvas::Context::IsValid() const
{
    return m_device_context != k_invalid_device_context_handle && m_graphics_context != k_invalid_graphics_context_handle;
}

#define RNDR_IS_EXTENSION_ALLOWED(extension)

bool Rndr::Canvas::Context::g_context_exists = false;
Rndr::Canvas::Context Rndr::Canvas::Context::Init(NativeWindowHandle window_handle)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Context::Init");

    if (g_context_exists)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Canvas::Context::Init called twice! Only one Context can exist at a time.");
    }

    Opal::Logger& logger = Opal::GetLogger();
    if (!logger.IsCategoryRegistered(k_log_category))
    {
        logger.RegisterCategory(k_log_category, Opal::LogLevel::Info);
    }

    Context ctx;

#if RNDR_WINDOWS
    if (window_handle == nullptr)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Window handle is null!");
    }

    ctx.m_device_context = GetDC(RNDR_TO_HWND(window_handle));
    if (ctx.m_device_context == k_invalid_device_context_handle)
    {
        throw GraphicsAPIException(0, "Failed to get device context from window!");
    }

    PIXELFORMATDESCRIPTOR pixel_format_desc = {};
    pixel_format_desc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pixel_format_desc.nVersion = 1;
    pixel_format_desc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixel_format_desc.iPixelType = PFD_TYPE_RGBA;
    pixel_format_desc.cColorBits = 32;
    pixel_format_desc.cAlphaBits = 8;
    pixel_format_desc.cDepthBits = 24;
    pixel_format_desc.cStencilBits = 8;

    const int pixel_format = ChoosePixelFormat(ctx.m_device_context, &pixel_format_desc);
    if (pixel_format == 0)
    {
        throw GraphicsAPIException(0, "Pixel format RGBA is not supported!");
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

    status = gladLoadWGL(ctx.m_device_context);
    if (status == 0)
    {
        wglDeleteContext(temp_context);
        throw GraphicsAPIException(0, "Failed to load WGL functions!");
    }

    // Create the real OpenGL 4.6 core profile context.
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

    HGLRC real_context = wglCreateContextAttribsARB(ctx.m_device_context, nullptr, attribute_list.GetData());
    if (real_context == nullptr)
    {
        wglDeleteContext(temp_context);
        throw GraphicsAPIException(0, "Failed to create OpenGL 4.6 core profile context!");
    }

    status = wglMakeCurrent(ctx.m_device_context, real_context);
    if (status == 0)
    {
        wglDeleteContext(real_context);
        wglDeleteContext(temp_context);
        throw GraphicsAPIException(0, "Failed to make OpenGL 4.6 context current!");
    }

    wglDeleteContext(temp_context);

    status = gladLoadGL();
    if (status == 0)
    {
        wglDeleteContext(real_context);
        throw GraphicsAPIException(0, "Failed to load OpenGL functions!");
    }

    ctx.m_graphics_context = real_context;

#if RNDR_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&DebugOutputCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif

#else
    RNDR_UNUSED(window_handle);
    throw GraphicsAPIException(0, "Platform not supported!");
#endif

    g_context_exists = true;
    logger.Info(k_log_category, "OpenGL 4.6 context initialized successfully.");
    return ctx;
}
