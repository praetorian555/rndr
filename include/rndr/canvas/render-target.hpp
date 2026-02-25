#pragma once

#include "rndr/canvas/context.hpp"
#include "rndr/canvas/texture.hpp"

namespace Rndr
{
namespace Canvas
{

/**
 * Off-screen surface you can draw to. Named RenderTarget instead of Framebuffer to avoid OpenGL
 * jargon. Result can be used as a texture for post-processing.
 */
class RenderTarget
{
public:
    RenderTarget() = default;
    explicit RenderTarget(i32 width, i32 height, Format format);
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    [[nodiscard]] RenderTarget Clone() const;
    void Destroy();

    /** @return The color attachment as a texture, usable for post-processing. */
    [[nodiscard]] const Texture& GetColorTexture() const;

    [[nodiscard]] i32 GetWidth() const;
    [[nodiscard]] i32 GetHeight() const;
    [[nodiscard]] Format GetFormat() const;
    [[nodiscard]] bool IsValid() const;

private:
    i32 m_width = 0;
    i32 m_height = 0;
    Format m_format = Format::RGBA8;
    Texture m_color_texture;
    u32 m_handle = 0;
};

}  // namespace Canvas
}  // namespace Rndr
