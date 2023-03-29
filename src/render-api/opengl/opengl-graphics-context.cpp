#include "rndr/render/opengl/opengl-graphics-context.h"

#if defined RNDR_WINDOWS
#include <windows.h>
#endif  // RNDR_WINDOWS

#if defined RNDR_OPENGL

#include <glad/glad.h>
#include <glad/glad_wgl.h>

#include "rndr/core/log.h"
#include "rndr/core/memory.h"
#include "rndr/render/buffer.h"
#include "rndr/render/commandlist.h"
#include "rndr/render/framebuffer.h"
#include "rndr/render/image.h"
#include "rndr/render/pipeline.h"
#include "rndr/render/sampler.h"
#include "rndr/render/shader.h"
#include "rndr/render/swapchain.h"

bool rndr::GraphicsContext::Init(rndr::GraphicsContextProperties props)
{
    properties = props;

#if RNDR_WINDOWS
    if (props.window_handle == nullptr)
    {
        RNDR_LOG_ERROR("GraphicsContext::Init: Window handle is null!");
        return false;
    }

    HDC device_context = GetDC(reinterpret_cast<HWND>(props.window_handle));
    if (device_context == nullptr)
    {
        RNDR_LOG_ERROR("GraphicsContext::Init: Failed to get device context from a native window!");
        return false;
    }
    gl_device_context = reinterpret_cast<void*>(device_context);

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

    const int pixel_format = ChoosePixelFormat(device_context, &pixel_format_desc);
    if (pixel_format == 0)
    {
        RNDR_LOG_ERROR("GraphicsContext::Init: Chosen pixel format does not exist!");
        return false;
    }
    BOOL status = SetPixelFormat(device_context, pixel_format, &pixel_format_desc);
    if (status == 0)
    {
        RNDR_LOG_ERROR(
            "GraphicsContext::Init: Failed to set new pixel format to the device context!");
        return false;
    }

    HGLRC graphics_context = wglCreateContext(device_context);
    if (graphics_context == nullptr)
    {
        RNDR_LOG_ERROR("GraphicsContext::Init: Failed to create OpenGL graphics context!");
        return false;
    }

    status = wglMakeCurrent(device_context, graphics_context);
    if (status == 0)
    {
        RNDR_LOG_ERROR("GraphicsContext::Init: Failed to make OpenGL graphics context current!");
        return false;
    }

    status = gladLoadWGL(device_context);
    if (status == 0)
    {
        RNDR_LOG_ERROR("GraphicsContext::Init: Failed to load WGL functions!");
        return false;
    }

    const int attribute_list[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB,
        4,
        WGL_CONTEXT_MINOR_VERSION_ARB,
        6,
        WGL_CONTEXT_FLAGS_ARB,
        WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB,
        WGL_CONTEXT_PROFILE_MASK_ARB,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0  // End of the attribute list
    };
    HGLRC old_graphics_context = graphics_context;
    graphics_context = wglCreateContextAttribsARB(device_context, graphics_context, attribute_list);
    if (graphics_context == nullptr)
    {
        RNDR_LOG_ERROR(
            "GraphicsContext::Init: Failed to make OpenGL graphics context with attribute list!");
        return false;
    }

    status = wglMakeCurrent(device_context, graphics_context);
    if (status == 0)
    {
        RNDR_LOG_ERROR("GraphicsContext::Init: Failed to make OpenGL graphics context current!");
        return false;
    }

    status = wglDeleteContext(old_graphics_context);
    if (status == 0)
    {
        RNDR_LOG_ERROR("GraphicsContext::Init: Failed to delete temporary graphics context!");
        return false;
    }

    status = gladLoadGL();
    if (status == 0)
    {
        RNDR_LOG_ERROR("GraphicsContext::Init: Failed to load OpenGL functions!");
        return false;
    }

    gl_graphics_context = reinterpret_cast<void*>(graphics_context);

    return true;
#else
    assert(false && "OS not supported!");
    return false;
#endif  // RNDR_WINDOWS
}

rndr::GraphicsContext::~GraphicsContext()
{
#if RNDR_WINDOWS
    HGLRC graphics_context = reinterpret_cast<HGLRC>(gl_graphics_context);
    const BOOL status = wglDeleteContext(graphics_context);
    if (status == 0)
    {
        RNDR_LOG_ERROR(
            "GraphicsContext::~GraphicsContext: Failed to destroy OpenGL graphics context!");
    }
#endif  // RNDR_WINDOWS
}
rndr::ScopePtr<rndr::SwapChain> rndr::GraphicsContext::CreateSwapChain(
    rndr::NativeWindowHandle window_handle,
    int width,
    int height,
    const rndr::SwapChainProperties& props)
{
    RNDR_UNUSED(window_handle);
    RNDR_UNUSED(width);
    RNDR_UNUSED(height);
    RNDR_UNUSED(props);
    return {};
}

rndr::ScopePtr<rndr::Shader> rndr::GraphicsContext::CreateShader(
    const rndr::ByteSpan& shader_contents,
    const rndr::ShaderProperties& props)
{
    RNDR_UNUSED(shader_contents);
    RNDR_UNUSED(props);
    return {};
}
rndr::ScopePtr<rndr::Image> rndr::GraphicsContext::CreateImage(int width,
                                                               int height,
                                                               const rndr::ImageProperties& props,
                                                               rndr::ByteSpan init_data)
{
    RNDR_UNUSED(width);
    RNDR_UNUSED(height);
    RNDR_UNUSED(props);
    RNDR_UNUSED(init_data);
    return {};
}
rndr::ScopePtr<rndr::Image> rndr::GraphicsContext::CreateImageArray(
    int width,
    int height,
    int array_size,
    const rndr::ImageProperties& props,
    rndr::Span<rndr::ByteSpan> init_data)
{
    RNDR_UNUSED(width);
    RNDR_UNUSED(height);
    RNDR_UNUSED(array_size);
    RNDR_UNUSED(props);
    RNDR_UNUSED(init_data);
    return {};
}
rndr::ScopePtr<rndr::Image> rndr::GraphicsContext::CreateCubeMap(
    int width,
    int height,
    const rndr::ImageProperties& props,
    rndr::Span<rndr::ByteSpan> init_data)
{
    RNDR_UNUSED(width);
    RNDR_UNUSED(height);
    RNDR_UNUSED(props);
    RNDR_UNUSED(init_data);
    return {};
}
rndr::ScopePtr<rndr::Image> rndr::GraphicsContext::CreateImageForSwapChain(
    rndr::SwapChain* swap_chain,
    int buffer_index)
{
    RNDR_UNUSED(swap_chain);
    RNDR_UNUSED(buffer_index);
    return {};
}
rndr::ScopePtr<rndr::Sampler> rndr::GraphicsContext::CreateSampler(const SamplerProperties& props)
{
    RNDR_UNUSED(props);
    return {};
}

rndr::ScopePtr<rndr::Buffer> rndr::GraphicsContext::CreateBuffer(
    const rndr::BufferProperties& props,
    rndr::ByteSpan initial_data)
{
    RNDR_UNUSED(props);
    RNDR_UNUSED(initial_data);
    return {};
}
rndr::ScopePtr<rndr::FrameBuffer> rndr::GraphicsContext::CreateFrameBuffer(
    int width,
    int height,
    const rndr::FrameBufferProperties& props)
{
    RNDR_UNUSED(width);
    RNDR_UNUSED(height);
    RNDR_UNUSED(props);
    return {};
}
rndr::ScopePtr<rndr::FrameBuffer> rndr::GraphicsContext::CreateFrameBufferForSwapChain(
    int width,
    int height,
    rndr::SwapChain* swap_chain)
{
    RNDR_UNUSED(width);
    RNDR_UNUSED(height);
    RNDR_UNUSED(swap_chain);
    return {};
}
rndr::ScopePtr<rndr::InputLayout> rndr::GraphicsContext::CreateInputLayout(
    rndr::Span<rndr::InputLayoutProperties> props,
    rndr::Shader* shader)
{
    RNDR_UNUSED(props);
    RNDR_UNUSED(shader);
    return {};
}
rndr::ScopePtr<rndr::RasterizerState> rndr::GraphicsContext::CreateRasterizerState(
    const rndr::RasterizerProperties& props)
{
    RNDR_UNUSED(props);
    return {};
}
rndr::ScopePtr<rndr::DepthStencilState> rndr::GraphicsContext::CreateDepthStencilState(
    const rndr::DepthStencilProperties& props)
{
    RNDR_UNUSED(props);
    return {};
}
rndr::ScopePtr<rndr::BlendState> rndr::GraphicsContext::CreateBlendState(
    const rndr::BlendProperties& props)
{
    RNDR_UNUSED(props);
    return {};
}
rndr::ScopePtr<rndr::Pipeline> rndr::GraphicsContext::CreatePipeline(
    const rndr::PipelineProperties& props)
{
    RNDR_UNUSED(props);
    return {};
}
rndr::ScopePtr<rndr::CommandList> rndr::GraphicsContext::CreateCommandList()
{
    return {};
}
bool rndr::GraphicsContext::SubmitCommandList(rndr::CommandList* list) const
{
    RNDR_UNUSED(list);
    return false;
}
void rndr::GraphicsContext::Present(rndr::SwapChain* swap_chain, bool activate_vsync) const
{
    RNDR_UNUSED(swap_chain);
    RNDR_UNUSED(activate_vsync);
    SwapBuffers(reinterpret_cast<HDC>(gl_device_context));
}

void rndr::GraphicsContext::ClearColor(rndr::Image* image, math::Vector4 color) const
{
    RNDR_UNUSED(image);
    RNDR_UNUSED(color);
}

#endif  // RNDR_OPENGL
