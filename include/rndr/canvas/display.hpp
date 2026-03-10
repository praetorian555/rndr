#pragma once

#include "opal/container/ref.h"

#include "rndr/canvas/context.hpp"

namespace Rndr
{
class GenericWindow;
}

namespace Rndr::Canvas
{

/**
 * Encapsulates swap chain, presentation, and vsync configuration. Replaces the concept of
 * "swap chain" with user-friendly naming. Represents the on-screen surface tied to a window.
 *
 * In OpenGL terms, the Display manages the default framebuffer (handle 0). Unlike a RenderTarget
 * (which wraps a user-created FBO with owned textures), the default framebuffer is owned by the
 * windowing system and cannot be read back as a texture. For this reason, Display does not expose
 * a RenderTarget. Instead, the DrawList will accept a Display directly to bind framebuffer 0 and
 * set the viewport from the Display's dimensions.
 *
 * Typical usage:
 * @code
 *   auto display = Canvas::Display(context, window, desc);
 *   // game loop
 *   list.SetRenderTarget(display);
 *   list.Draw(mesh, brush);
 *   list.Execute();
 *   display.Present();
 *   // on resize callback
 *   display.Resize(new_width, new_height);
 * @endcode
 */
class Display
{
public:
    Display() = default;

    /**
     * Create a display surface tied to a window.
     * @param context Active Canvas context (must outlive the Display).
     * @param window Window to present to.
     * @param vsync_enabled If vsync should be enabled or not. Defaults to true.
     */
    explicit Display(const Context& context, Opal::Ref<GenericWindow> window, bool vsync_enabled = true);
    ~Display();

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;
    Display(Display&& other) noexcept;
    Display& operator=(Display&& other) noexcept;

    void Destroy();

    /** Swap front and back buffers, presenting the current frame to the screen. */
    void Present();

    /** Enable or disable vertical sync without teardown. */
    void SetVsync(bool enabled);

    /**
     * Update the stored surface dimensions. Call this from your window resize callback.
     * Does not perform any GL calls — the DrawList uses these dimensions for glViewport.
     */
    void Resize(i32 width, i32 height);

    /** @return Width of the display surface in pixels. */
    [[nodiscard]] i32 GetWidth() const;

    /** @return Height of the display surface in pixels. */
    [[nodiscard]] i32 GetHeight() const;

    /** @return True if vsync is enabled, false otherwise. */
    [[nodiscard]] bool IsVsyncEnabled() const;

    [[nodiscard]] bool IsValid() const;

private:
    bool m_vsync_enabled = true;
    Opal::Ref<GenericWindow> m_window = nullptr;
    NativeDeviceContextHandle m_device_context = k_invalid_device_context_handle;
    i32 m_width = 0;
    i32 m_height = 0;
};

}  // namespace Rndr::Canvas