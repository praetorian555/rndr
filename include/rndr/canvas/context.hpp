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
 * Created exclusively through the CreateContext() factory. RAII: destructor tears down the GL backend.
 *
 * In OpenGL terms, the Context manages the default framebuffer (handle 0). Unlike a RenderTarget
 * (which wraps a user-created FBO with owned textures), the default framebuffer is owned by the
 * windowing system. The DrawList accepts a Context directly to bind framebuffer 0 and set the
 * viewport from the Context's dimensions.
 *
 * The first CreateContext() call creates the primary context; later calls create secondary contexts
 * that share GPU resources with it. See CreateContext() for details and the threading/sync rules.
 *
 * Typical usage:
 * @code
 *   auto context = Canvas::Context::CreateContext(window, desc);
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
     * Create a Canvas context. Behavior depends on whether a context already exists and whether a
     * window is supplied:
     *
     * 1. First call (no context yet), @p window valid -> the PRIMARY context. Initializes the OpenGL
     *    backend, binds to @p window, owns its on-screen surface. @p desc sets color/depth formats and
     *    vsync.
     * 2. Later call, @p window valid -> a SECONDARY per-window context. Owns @p window's surface (can
     *    present) and shares GPU resources with the primary -- so a multi-window app uploads shaders,
     *    glyph atlases, textures once and uses them in every window. @p desc configures this window's
     *    surface; for resource sharing to work its pixel format must be compatible with the primary's
     *    (typically identical). The returned context is left current on the calling thread.
     * 3. Later call, no @p window -> a RESOURCE-ONLY context that borrows the primary's surface and
     *    shares its resources. Intended for off-thread resource work; must not be used to present.
     *
     * Shared across contexts: textures, buffers, renderbuffers, samplers, and shader/program objects.
     * NOT shared (must be created per-context): VAOs, FBOs, transform-feedback objects, program
     * pipelines, and queries -- so RenderTargets and DrawLists are not portable between contexts even
     * though the textures they reference are.
     *
     * An OpenGL context can be current on at most one thread at a time. A resource-only context (case
     * 3) is NOT made current; call MakeCurrent() on the thread that will use it. A multi-window app
     * binds the relevant window's context (MakeCurrent) before rendering or presenting that window.
     * @code
     *   // Main thread:
     *   auto main_window = Canvas::Context::CreateContext(window_a, desc);   // primary
     *   auto tool_window = Canvas::Context::CreateContext(window_b, desc);   // shares with primary
     *   auto loader      = Canvas::Context::CreateContext();                 // resource-only
     *
     *   // Per-window render: bind the window's context, draw into it, then present it.
     *   main_window.MakeCurrent();
     *   // ... draw main window ...
     *   main_window.Present();
     *   tool_window.MakeCurrent();
     *   // ... draw tool window ...
     *   tool_window.Present();
     * @endcode
     *
     * Sharing the namespace does not guarantee memory coherency: after writing a resource in one
     * context, synchronize (glFinish, or a glFenceSync + glWaitSync/glClientWaitSync pair) before
     * reading it from another context.
     *
     * @param window Window to bind. Required for the primary and for per-window contexts; omit for a
     *               resource-only context.
     * @param desc Configuration for the context's surface. Ignored for a resource-only context.
     * @return A valid Context.
     * @throw Opal::InvalidArgumentException if no primary exists yet and @p window is null.
     * @throw Rndr::GraphicsAPIException if the OpenGL backend or context creation fails.
     */
    [[nodiscard]] static Context CreateContext(Opal::Ref<GenericWindow> window = nullptr, const ContextDesc& desc = {});

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
     * Make this context current on the calling thread. Required before issuing GL commands from a
     * thread other than the one that created/last bound the context (e.g. a worker thread using a
     * shared context from CreateContext()).
     * @return True on success, false otherwise.
     */
    bool MakeCurrent();

    /**
     * Release any current context from the calling thread. Call before the thread exits, or before
     * a context is made current on a different thread.
     * @return True on success, false otherwise.
     */
    static bool ReleaseCurrent();

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

    // Handles of the active primary context, tracked so a later CreateContext() call can build shared
    // contexts that reach them. Handle values are stable across C++ moves, so no per-move maintenance.
    static NativeDeviceContextHandle g_primary_device_context;
    static NativeGraphicsContextHandle g_primary_graphics_context;

    bool m_vsync_enabled = true;
    // True for the primary context (owns the window surface and the singleton slot). False for shared
    // contexts, which borrow the primary's device context and must not present.
    bool m_owns_surface = true;
    Format m_color_format = Format::RGBA8;
    Format m_depth_stencil_format = Format::D24S8;
    Opal::Ref<GenericWindow> m_window = nullptr;
    NativeDeviceContextHandle m_device_context = k_invalid_device_context_handle;
    NativeGraphicsContextHandle m_graphics_context = k_invalid_graphics_context_handle;
    i32 m_width = 0;
    i32 m_height = 0;
};

}  // namespace Rndr::Canvas