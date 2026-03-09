#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/canvas/context.hpp"
#include "rndr/canvas/texture.hpp"

namespace Rndr
{
namespace Canvas
{

/** Maximum number of color attachments per render target. */
constexpr i32 k_max_color_attachments = 4;

struct RenderTargetDesc
{
    /** Color attachment descriptions. At least one is required. */
    Opal::DynamicArray<TextureDesc> color_attachments;

    /** Whether to create a depth/stencil attachment. */
    bool use_depth_stencil = false;

    /** Depth/stencil attachment description. Only used when use_depth_stencil is true. */
    TextureDesc depth_stencil_attachment;

    /** Add a color attachment with the given dimensions and format. */
    RenderTargetDesc& AddColor(i32 width, i32 height, Format format = Format::RGBA8)
    {
        TextureDesc desc;
        desc.width = width;
        desc.height = height;
        desc.format = format;
        color_attachments.PushBack(desc);
        return *this;
    }

    /** Add a color attachment from a full TextureDesc. */
    RenderTargetDesc& AddColor(const TextureDesc& desc)
    {
        color_attachments.PushBack(desc);
        return *this;
    }

    /** Add a depth/stencil attachment with the given dimensions. */
    RenderTargetDesc& SetDepthStencil(i32 width, i32 height, Format format = Format::D24S8)
    {
        use_depth_stencil = true;
        depth_stencil_attachment.width = width;
        depth_stencil_attachment.height = height;
        depth_stencil_attachment.format = format;
        return *this;
    }
};

/**
 * Off-screen surface you can draw to. Named RenderTarget instead of Framebuffer to avoid OpenGL
 * jargon. Color attachments can be used as textures for post-processing.
 */
class RenderTarget
{
public:
    RenderTarget() = default;

    /**
     * Create a render target.
     * @param context Active Canvas context.
     * @param desc Render target descriptor.
     * @param name Debug name for GPU debugging tools.
     */
    explicit RenderTarget(const Context& context, const RenderTargetDesc& desc, const Opal::StringUtf8& name = {});
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    [[nodiscard]] RenderTarget Clone() const;
    void Destroy();

    [[nodiscard]] i32 GetColorAttachmentCount() const;
    [[nodiscard]] const Texture& GetColorAttachment(i32 index) const;
    [[nodiscard]] const Texture& GetDepthStencilAttachment() const;

    [[nodiscard]] i32 GetWidth() const;
    [[nodiscard]] i32 GetHeight() const;
    [[nodiscard]] u32 GetNativeHandle() const;
    [[nodiscard]] const Opal::StringUtf8& GetName() const;
    [[nodiscard]] bool IsValid() const;

private:
    Opal::DynamicArray<Texture> m_color_attachments;
    Texture m_depth_stencil_attachment;
    bool m_use_depth_stencil = false;
    u32 m_handle = 0;
    Opal::StringUtf8 m_name;
};

}  // namespace Canvas
}  // namespace Rndr
