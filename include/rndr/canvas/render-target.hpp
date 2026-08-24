#pragma once

#include <tuple>

#include "opal/clonable-base.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/ref.h"
#include "opal/container/string.h"

#include "rndr/canvas/format.hpp"
#include "rndr/canvas/texture.hpp"

namespace Rndr
{
namespace Canvas
{

/** Maximum number of color attachments per render target. */
constexpr i32 k_max_color_attachments = 4;

/**
 * Describes one render target attachment. Either the render target creates the texture, using the
 * inherited TextureDesc fields, or it renders into an existing texture, which the caller owns and
 * must keep alive for the lifetime of the render target.
 */
struct RenderTargetAttachmentDesc : TextureDesc, Opal::ClonableBase<RenderTargetAttachmentDesc>
{
    /** Existing texture to render into. When null, the render target creates and owns the texture. */
    Opal::Ref<const Texture> texture = nullptr;

    /** Mip level of texture to attach. Only used when texture is set. */
    i32 mip_level = 0;

    /** Array layer or cube map face of texture to attach. Negative attaches the whole texture. */
    i32 layer = -1;

    // Written out rather than using OPAL_CLONE_FIELDS, which cannot tie the TextureDesc sub-object:
    // the macro generates both accessors from one field list, and the non-const one needs a mutable
    // reference to it.
    auto GetFields() const { return std::tie(static_cast<const TextureDesc&>(*this), texture, mip_level, layer); }
    auto GetFields() { return std::tie(static_cast<TextureDesc&>(*this), texture, mip_level, layer); }
};

struct RenderTargetDesc
{
    /** Color attachment descriptions. At least one is required unless use_depth_stencil is set. */
    Opal::DynamicArray<RenderTargetAttachmentDesc> color_attachments;

    /** Whether to attach a depth/stencil attachment. */
    bool use_depth_stencil = false;

    /** Depth/stencil attachment description. Only used when use_depth_stencil is true. */
    RenderTargetAttachmentDesc depth_stencil_attachment;

    /** Add a color attachment with the given dimensions and format. */
    RenderTargetDesc& AddColor(i32 width, i32 height, Format format = Format::RGBA8)
    {
        RenderTargetAttachmentDesc desc;
        desc.width = width;
        desc.height = height;
        desc.format = format;
        color_attachments.PushBack(std::move(desc));
        return *this;
    }

    /** Add a color attachment from a full TextureDesc. */
    RenderTargetDesc& AddColor(const TextureDesc& desc)
    {
        RenderTargetAttachmentDesc attachment;
        static_cast<TextureDesc&>(attachment) = desc;
        color_attachments.PushBack(std::move(attachment));
        return *this;
    }

    /**
     * Add a color attachment that renders into an existing texture owned by the caller. The texture
     * must outlive the render target.
     * @param texture Texture to render into.
     * @param mip_level Mip level to render into.
     * @param layer Array layer or cube map face to render into. Negative renders into the whole texture.
     */
    RenderTargetDesc& AddColor(const Texture& texture, i32 mip_level = 0, i32 layer = -1)
    {
        RenderTargetAttachmentDesc attachment;
        attachment.texture = &texture;
        attachment.mip_level = mip_level;
        attachment.layer = layer;
        color_attachments.PushBack(std::move(attachment));
        return *this;
    }

    /** Add a depth/stencil attachment with the given dimensions. */
    RenderTargetDesc& SetDepthStencil(i32 width, i32 height, Format format = Format::D24S8)
    {
        use_depth_stencil = true;
        depth_stencil_attachment = RenderTargetAttachmentDesc{};
        depth_stencil_attachment.width = width;
        depth_stencil_attachment.height = height;
        depth_stencil_attachment.format = format;
        return *this;
    }

    /**
     * Render depth/stencil into an existing texture owned by the caller. The texture must outlive the
     * render target.
     * @param texture Depth or depth/stencil texture to render into.
     * @param mip_level Mip level to render into.
     * @param layer Array layer or cube map face to render into. Negative renders into the whole texture.
     */
    RenderTargetDesc& SetDepthStencil(const Texture& texture, i32 mip_level = 0, i32 layer = -1)
    {
        use_depth_stencil = true;
        depth_stencil_attachment = RenderTargetAttachmentDesc{};
        depth_stencil_attachment.texture = &texture;
        depth_stencil_attachment.mip_level = mip_level;
        depth_stencil_attachment.layer = layer;
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
     * Create a render target. Requires an active Canvas context on the calling thread.
     * @param desc Render target descriptor.
     * @param name Debug name for GPU debugging tools.
     * @return The render target, ErrorCode::InvalidArgument for a desc with no attachments, too many of
     *         them, or an external attachment that does not fit its texture, ErrorCode::OutOfBounds for a
     *         mip level or layer that does not exist, or ErrorCode::GraphicsAPIError when GL refuses the
     *         framebuffer. The reason is logged at error level.
     */
    [[nodiscard]] static Opal::Expected<RenderTarget, ErrorCode> Create(const RenderTargetDesc& desc,
                                                                        const Opal::StringUtf8& name = {});
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    /**
     * Copy this render target into a new one. Owned attachments are cloned; external ones stay borrowed.
     * @return The clone, ErrorCode::InvalidArgument for an invalid render target, or the code the failing
     *         step maps to.
     */
    [[nodiscard]] Opal::Expected<RenderTarget, ErrorCode> Clone() const;
    void Destroy();

    [[nodiscard]] i32 GetColorAttachmentCount() const;
    [[nodiscard]] const Texture& GetColorAttachment(i32 index) const;
    [[nodiscard]] const Texture& GetDepthStencilAttachment() const;

    /** Whether the color attachment is a texture owned by the caller rather than by the render target. */
    [[nodiscard]] bool IsColorAttachmentExternal(i32 index) const;

    /** Whether the depth/stencil attachment is a texture owned by the caller. */
    [[nodiscard]] bool IsDepthStencilExternal() const;

    [[nodiscard]] i32 GetWidth() const;
    [[nodiscard]] i32 GetHeight() const;
    [[nodiscard]] u32 GetNativeHandle() const;
    [[nodiscard]] const Opal::StringUtf8& GetName() const;
    [[nodiscard]] bool IsValid() const;

private:
    /** One attachment, either created and owned by this render target or borrowed from the caller. */
    struct Attachment
    {
        /** Texture created by this render target. Valid only when external is null. */
        Texture owned;

        /** Texture owned by the caller. Non-null when the attachment is borrowed. */
        Opal::Ref<const Texture> external = nullptr;

        i32 mip_level = 0;
        i32 layer = -1;

        [[nodiscard]] const Texture& Get() const { return external.IsValid() ? external.Get() : owned; }
        [[nodiscard]] bool IsExternal() const { return external.IsValid(); }
    };

    /** Throw if the attached mip levels do not all have the same dimensions and sample count. */
    [[nodiscard]] ErrorCode ValidateAttachmentSizes() const;

    Opal::DynamicArray<Attachment> m_color_attachments;
    Attachment m_depth_stencil_attachment;
    bool m_use_depth_stencil = false;
    u32 m_handle = 0;
    Opal::StringUtf8 m_name;
};

}  // namespace Canvas
}  // namespace Rndr
