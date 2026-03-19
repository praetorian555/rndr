#pragma once

#include "opal/container/ref.h"

#include "rndr/platform/windows-forward-def.hpp"
#include "rndr/types.hpp"

namespace Rndr
{
class GenericWindow;
}

namespace Rndr::Canvas
{

/**
 * Simplified data format enum covering both pixel formats and vertex attribute formats.
 * Canvas uses its own format vocabulary instead of exposing raw API-level formats.
 */
enum class Format : u8
{
    // Pixel formats.
    R8,
    RG8,
    RGB8,
    RGBA8,
    SRGB8,
    SRGBA8,
    R16F,
    RG16F,
    RGBA16F,
    R32F,
    RG32F,
    RGBA32F,
    D24S8,
    D32F,

    // Vertex data formats.
    Float1,
    Float2,
    Float3,
    Float4,
    Int1,
    Int2,
    Int3,
    Int4,

    EnumCount
};

/**
 * Configuration for the Canvas context, including color and depth/stencil formats.
 */
struct ContextDesc
{
    /** Color buffer format. Must be a color pixel format (e.g. RGBA8, RGB8). */
    Format color_format = Format::RGBA8;

    /** Depth/stencil buffer format. Must be D24S8 or D32F. */
    Format depth_stencil_format = Format::D24S8;

    /** Whether vertical sync should be enabled. */
    bool vsync_enabled = true;
};

/**
 * Represents the graphics backend being alive and the on-screen presentation surface.
 * Created exclusively through the Init() factory. RAII: destructor tears down the GL backend.
 *
 * In OpenGL terms, the Context manages the default framebuffer (handle 0). Unlike a RenderTarget
 * (which wraps a user-created FBO with owned textures), the default framebuffer is owned by the
 * windowing system. The DrawList accepts a Context directly to bind framebuffer 0 and set the
 * viewport from the Context's dimensions.
 *
 * Typical usage:
 * @code
 *   auto context = Canvas::Context::Init(window, desc);
 *   // game loop
 *   list.SetRenderTarget(context);
 *   list.Draw(mesh, brush);
 *   list.Execute();
 *   context.Present();
 *   // on resize callback
 *   context.Resize(new_width, new_height);
 * @endcode
 */
class Context
{
public:
    /**
     * Initialize the Canvas graphics backend.
     * @param window Window to bind the GL context to.
     * @param desc Configuration for the context.
     * @return A valid Context object.
     * @throw Opal::InvalidArgumentException if called while a Context already exists.
     * @throw Rndr::GraphicsAPIException if the OpenGL backend fails to initialize.
     */
    [[nodiscard]] static Context Init(Opal::Ref<GenericWindow> window, const ContextDesc& desc = {});

    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&& other) noexcept;
    Context& operator=(Context&& other) noexcept;

    void Destroy();

    /** Swap front and back buffers, presenting the current frame to the screen. */
    void Present();

    /** Enable or disable vertical sync without teardown. */
    void SetVsync(bool enabled);

    /**
     * Update the stored surface dimensions. Call this from your window resize callback.
     * Does not perform any GL calls -- the DrawList uses these dimensions for glViewport.
     */
    void Resize(i32 width, i32 height);

    /** @return Width of the display surface in pixels. */
    [[nodiscard]] i32 GetWidth() const;

    /** @return Height of the display surface in pixels. */
    [[nodiscard]] i32 GetHeight() const;

    /** @return True if vsync is enabled, false otherwise. */
    [[nodiscard]] bool IsVsyncEnabled() const;

    /** @return The color format configured for this context. */
    [[nodiscard]] Format GetColorFormat() const;

    /** @return The depth/stencil format configured for this context. */
    [[nodiscard]] Format GetDepthStencilFormat() const;

    [[nodiscard]] bool IsValid() const;

private:
    Context();

    static bool g_context_exists;
    bool m_vsync_enabled = true;
    Format m_color_format = Format::RGBA8;
    Format m_depth_stencil_format = Format::D24S8;
    Opal::Ref<GenericWindow> m_window = nullptr;
    NativeDeviceContextHandle m_device_context = k_invalid_device_context_handle;
    NativeGraphicsContextHandle m_graphics_context = k_invalid_graphics_context_handle;
    i32 m_width = 0;
    i32 m_height = 0;
};

}  // namespace Rndr::Canvas