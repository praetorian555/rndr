#pragma once

#include "rndr/canvas/render-target.hpp"

namespace Rndr
{
namespace Canvas
{

struct DisplayDesc
{
    /** Whether vertical sync is enabled. */
    bool vsync = true;

    /** Pixel format of the display surface. */
    Format format = Format::RGBA8;
};

/**
 * Encapsulates swap chain, presentation, and vsync configuration. Replaces the concept of
 * "swap chain" with user-friendly naming. Provides a RenderTarget via GetTarget() for drawing
 * to screen. Supports reconfiguration without teardown.
 */
class Display
{
public:
    Display() = default;
    explicit Display(const Context& context, NativeWindowHandle window, const DisplayDesc& desc = {});
    ~Display();

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;
    Display(Display&& other) noexcept;
    Display& operator=(Display&& other) noexcept;

    [[nodiscard]] Display Clone() const;
    void Destroy();

    /** Present the current frame to the screen. */
    void Present();

    /** Enable or disable vertical sync without teardown. */
    void SetVsync(bool enabled);

    /** Resize the display surface without teardown. */
    void Resize(i32 width, i32 height);

    /** @return The on-screen render target. */
    [[nodiscard]] const RenderTarget& GetTarget() const;

    [[nodiscard]] const DisplayDesc& GetDesc() const;
    [[nodiscard]] bool IsValid() const;

private:
    DisplayDesc m_desc;
    RenderTarget m_target;
};

}  // namespace Canvas
}  // namespace Rndr
