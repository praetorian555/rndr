#pragma once

#include "rndr/canvas/context.hpp"

namespace Rndr
{
namespace Canvas
{

struct TextureDesc
{
    /** Width of the texture in pixels. */
    i32 width = 0;

    /** Height of the texture in pixels. */
    i32 height = 0;

    /** Pixel format of the texture. */
    Format format = Format::RGBA8;
};

/**
 * GPU texture resource. Move-only, RAII.
 */
class Texture
{
public:
    Texture() = default;
    explicit Texture(const Context& context, const TextureDesc& desc = {});
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    [[nodiscard]] Texture Clone() const;
    void Destroy();

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] const TextureDesc& GetDesc() const;

private:
    TextureDesc m_desc;
    u32 m_handle = 0;
};

}  // namespace Canvas
}  // namespace Rndr
