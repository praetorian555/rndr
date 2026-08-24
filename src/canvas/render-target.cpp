#include "rndr/canvas/render-target.hpp"

#include "glad/glad.h"

#include "rndr/canvas/gl-result.hpp"
#include "rndr/log.hpp"
#include "rndr/trace.hpp"

namespace
{

bool IsDepthFormat(Rndr::Canvas::Format format)
{
    return format == Rndr::Canvas::Format::D24S8 || format == Rndr::Canvas::Format::D32F;
}

/** Dimension of a mip level, halving per level and clamping at 1. */
Rndr::i32 MipSize(Rndr::i32 base_size, Rndr::i32 mip_level)
{
    const Rndr::i32 size = base_size >> mip_level;
    return size > 1 ? size : 1;
}

/** Number of layers that can be selected with the layer field, or 0 when the type has no layers. */
Rndr::i32 GetLayerCount(const Rndr::Canvas::TextureDesc& desc)
{
    switch (desc.type)
    {
        case Rndr::Canvas::TextureType::Texture2DArray:
            return desc.array_size;
        case Rndr::Canvas::TextureType::CubeMap:
            return 6;
        default:
            return 0;
    }
}

/**
 * Check an attachment description that points at a caller-owned texture. Descriptions that create
 * their own texture need no checking here, Texture::Create does it.
 */
Rndr::ErrorCode ValidateExternalAttachment(const Rndr::Canvas::RenderTargetAttachmentDesc& desc, bool is_depth)
{
    const Rndr::Canvas::Texture& texture = desc.texture.Get();
    if (!texture.IsValid())
    {
        RNDR_LOG_ERROR("Canvas: External attachment texture is not valid");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (is_depth && !IsDepthFormat(texture.GetDesc().format))
    {
        RNDR_LOG_ERROR("Canvas: Depth/stencil attachment needs a depth format texture");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (!is_depth && IsDepthFormat(texture.GetDesc().format))
    {
        RNDR_LOG_ERROR("Canvas: Color attachment can not use a depth format texture");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (desc.mip_level < 0 || desc.mip_level >= texture.GetMipLevelCount())
    {
        RNDR_LOG_ERROR("Canvas: Attachment mip level is out of bounds");
        return Rndr::ErrorCode::OutOfBounds;
    }
    if (desc.layer >= 0)
    {
        const Rndr::i32 layer_count = GetLayerCount(texture.GetDesc());
        if (layer_count == 0)
        {
            RNDR_LOG_ERROR("Canvas: Attachment texture has no layers to select");
            return Rndr::ErrorCode::InvalidArgument;
        }
        if (desc.layer >= layer_count)
        {
            RNDR_LOG_ERROR("Canvas: Attachment layer is out of bounds");
            return Rndr::ErrorCode::OutOfBounds;
        }
    }
    return Rndr::ErrorCode::Success;
}

/** Attach the whole texture, or a single layer or cube map face of it, to an attachment point. */
void AttachTexture(Rndr::u32 framebuffer, GLenum attachment_point, const Rndr::Canvas::Texture& texture, Rndr::i32 mip_level,
                   Rndr::i32 layer)
{
    if (layer >= 0)
    {
        glNamedFramebufferTextureLayer(framebuffer, attachment_point, texture.GetNativeHandle(), mip_level, layer);
    }
    else
    {
        glNamedFramebufferTexture(framebuffer, attachment_point, texture.GetNativeHandle(), mip_level);
    }
}

/**
 * Point the draw and read buffers at the color attachments. Without this only attachment 0 receives
 * output, and a depth-only framebuffer is incomplete because it has no color buffer to draw to.
 */
void SetDrawBuffers(Rndr::u32 framebuffer, Rndr::i32 color_attachment_count)
{
    if (color_attachment_count == 0)
    {
        glNamedFramebufferDrawBuffer(framebuffer, GL_NONE);
        glNamedFramebufferReadBuffer(framebuffer, GL_NONE);
        return;
    }

    GLenum draw_buffers[Rndr::Canvas::k_max_color_attachments] = {};
    for (Rndr::i32 i = 0; i < color_attachment_count; ++i)
    {
        draw_buffers[i] = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
    }
    glNamedFramebufferDrawBuffers(framebuffer, color_attachment_count, draw_buffers);
}

}  // namespace

Opal::Expected<Rndr::Canvas::RenderTarget, Rndr::ErrorCode> Rndr::Canvas::RenderTarget::Create(const RenderTargetDesc& desc,
                                                                                               const Opal::StringUtf8& name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::RenderTarget::Create");
    using ResultType = Opal::Expected<RenderTarget, ErrorCode>;

    if (desc.color_attachments.IsEmpty() && !desc.use_depth_stencil)
    {
        RNDR_LOG_ERROR("Canvas: A color or a depth/stencil attachment is required");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (desc.color_attachments.GetSize() > k_max_color_attachments)
    {
        RNDR_LOG_ERROR("Canvas: Too many color attachments");
        return ResultType(ErrorCode::InvalidArgument);
    }

    for (const RenderTargetAttachmentDesc& attachment_desc : desc.color_attachments)
    {
        if (attachment_desc.texture.IsValid())
        {
            RNDR_CANVAS_CHECK_EXPECTED(ValidateExternalAttachment(attachment_desc, false), ResultType);
        }
    }
    if (desc.use_depth_stencil && desc.depth_stencil_attachment.texture.IsValid())
    {
        RNDR_CANVAS_CHECK_EXPECTED(ValidateExternalAttachment(desc.depth_stencil_attachment, true), ResultType);
    }

    // On failure beyond this point, render_target's destructor releases the framebuffer and any
    // attachments it created.
    RenderTarget render_target;
    render_target.m_use_depth_stencil = desc.use_depth_stencil;
    render_target.m_name = name.Clone();

    glCreateFramebuffers(1, &render_target.m_handle);
    if (render_target.m_handle == 0)
    {
        RNDR_LOG_ERROR("Canvas: Failed to create GL framebuffer");
        return ResultType(ErrorCode::GraphicsAPIError);
    }

    for (i32 i = 0; i < static_cast<i32>(desc.color_attachments.GetSize()); ++i)
    {
        const RenderTargetAttachmentDesc& attachment_desc = desc.color_attachments[i];
        Attachment attachment;
        attachment.mip_level = attachment_desc.mip_level;
        attachment.layer = attachment_desc.layer;
        if (attachment_desc.texture.IsValid())
        {
            attachment.external = attachment_desc.texture.GetPtr();
        }
        else
        {
            attachment.mip_level = 0;
            attachment.layer = -1;
            auto owned_result = Texture::Create(static_cast<const TextureDesc&>(attachment_desc));
            RNDR_CANVAS_CHECK_EXPECTED(owned_result.GetErrorOr(ErrorCode::Success), ResultType);
            attachment.owned = std::move(owned_result).GetValue();
        }
        AttachTexture(render_target.m_handle, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i), attachment.Get(), attachment.mip_level,
                      attachment.layer);
        render_target.m_color_attachments.PushBack(std::move(attachment));
    }

    if (desc.use_depth_stencil)
    {
        const RenderTargetAttachmentDesc& attachment_desc = desc.depth_stencil_attachment;
        render_target.m_depth_stencil_attachment.mip_level = attachment_desc.mip_level;
        render_target.m_depth_stencil_attachment.layer = attachment_desc.layer;
        if (attachment_desc.texture.IsValid())
        {
            render_target.m_depth_stencil_attachment.external = attachment_desc.texture.GetPtr();
        }
        else
        {
            render_target.m_depth_stencil_attachment.mip_level = 0;
            render_target.m_depth_stencil_attachment.layer = -1;
            auto owned_result = Texture::Create(static_cast<const TextureDesc&>(attachment_desc));
            RNDR_CANVAS_CHECK_EXPECTED(owned_result.GetErrorOr(ErrorCode::Success), ResultType);
            render_target.m_depth_stencil_attachment.owned = std::move(owned_result).GetValue();
        }
        const Texture& depth_texture = render_target.m_depth_stencil_attachment.Get();
        const GLenum attachment_point =
            depth_texture.GetDesc().format == Format::D24S8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
        AttachTexture(render_target.m_handle, attachment_point, depth_texture, render_target.m_depth_stencil_attachment.mip_level,
                      render_target.m_depth_stencil_attachment.layer);
    }

    RNDR_CANVAS_CHECK_EXPECTED(render_target.ValidateAttachmentSizes(), ResultType);
    SetDrawBuffers(render_target.m_handle, render_target.GetColorAttachmentCount());

    const GLenum status = glCheckNamedFramebufferStatus(render_target.m_handle, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        RNDR_LOG_ERROR("Canvas: Framebuffer is not complete: {}", status);
        return ResultType(ErrorCode::GraphicsAPIError);
    }

    if (!render_target.m_name.IsEmpty())
    {
        glObjectLabel(GL_FRAMEBUFFER, render_target.m_handle, static_cast<GLsizei>(render_target.m_name.GetSize()),
                      render_target.m_name.GetData());
    }
    return ResultType(std::move(render_target));
}

Rndr::Canvas::RenderTarget::~RenderTarget()
{
    Destroy();
}

Rndr::Canvas::RenderTarget::RenderTarget(RenderTarget&& other) noexcept
    : m_color_attachments(std::move(other.m_color_attachments)),
      m_depth_stencil_attachment(std::move(other.m_depth_stencil_attachment)),
      m_use_depth_stencil(other.m_use_depth_stencil),
      m_handle(other.m_handle),
      m_name(std::move(other.m_name))
{
    other.m_handle = 0;
    other.m_use_depth_stencil = false;
}

Rndr::Canvas::RenderTarget& Rndr::Canvas::RenderTarget::operator=(RenderTarget&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_color_attachments = std::move(other.m_color_attachments);
        m_depth_stencil_attachment = std::move(other.m_depth_stencil_attachment);
        m_use_depth_stencil = other.m_use_depth_stencil;
        m_handle = other.m_handle;
        m_name = std::move(other.m_name);
        other.m_handle = 0;
        other.m_use_depth_stencil = false;
    }
    return *this;
}

Opal::Expected<Rndr::Canvas::RenderTarget, Rndr::ErrorCode> Rndr::Canvas::RenderTarget::Clone() const
{
    using ResultType = Opal::Expected<RenderTarget, ErrorCode>;
    if (!IsValid())
    {
        RNDR_LOG_ERROR("Canvas: Cannot clone an invalid render target");
        return ResultType(ErrorCode::InvalidArgument);
    }

    RenderTarget clone;
    clone.m_use_depth_stencil = m_use_depth_stencil;
    clone.m_name = m_name.Clone();

    glCreateFramebuffers(1, &clone.m_handle);
    if (clone.m_handle == 0)
    {
        RNDR_LOG_ERROR("Canvas: Failed to create GL framebuffer for clone");
        return ResultType(ErrorCode::GraphicsAPIError);
    }

    for (i32 i = 0; i < static_cast<i32>(m_color_attachments.GetSize()); ++i)
    {
        const Attachment& source = m_color_attachments[i];
        Attachment attachment;
        attachment.mip_level = source.mip_level;
        attachment.layer = source.layer;
        if (source.IsExternal())
        {
            // A clone never takes ownership of a texture it did not create, it keeps borrowing it.
            attachment.external = source.external.GetPtr();
        }
        else
        {
            auto owned_clone_result = source.owned.Clone();
            RNDR_CANVAS_CHECK_EXPECTED(owned_clone_result.GetErrorOr(ErrorCode::Success), ResultType);
            attachment.owned = std::move(owned_clone_result).GetValue();
        }
        AttachTexture(clone.m_handle, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i), attachment.Get(), attachment.mip_level,
                      attachment.layer);
        clone.m_color_attachments.PushBack(std::move(attachment));
    }

    if (m_use_depth_stencil && m_depth_stencil_attachment.Get().IsValid())
    {
        clone.m_depth_stencil_attachment.mip_level = m_depth_stencil_attachment.mip_level;
        clone.m_depth_stencil_attachment.layer = m_depth_stencil_attachment.layer;
        if (m_depth_stencil_attachment.IsExternal())
        {
            clone.m_depth_stencil_attachment.external = m_depth_stencil_attachment.external.GetPtr();
        }
        else
        {
            auto depth_clone_result = m_depth_stencil_attachment.owned.Clone();
            RNDR_CANVAS_CHECK_EXPECTED(depth_clone_result.GetErrorOr(ErrorCode::Success), ResultType);
            clone.m_depth_stencil_attachment.owned = std::move(depth_clone_result).GetValue();
        }
        const Texture& depth_texture = clone.m_depth_stencil_attachment.Get();
        const GLenum attachment_point =
            depth_texture.GetDesc().format == Format::D24S8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
        AttachTexture(clone.m_handle, attachment_point, depth_texture, clone.m_depth_stencil_attachment.mip_level,
                      clone.m_depth_stencil_attachment.layer);
    }

    SetDrawBuffers(clone.m_handle, clone.GetColorAttachmentCount());

    if (!clone.m_name.IsEmpty())
    {
        glObjectLabel(GL_FRAMEBUFFER, clone.m_handle, static_cast<GLsizei>(clone.m_name.GetSize()), clone.m_name.GetData());
    }

    return ResultType(std::move(clone));
}

void Rndr::Canvas::RenderTarget::Destroy()
{
    if (m_handle != 0)
    {
        glDeleteFramebuffers(1, &m_handle);
        m_handle = 0;
    }
    for (Attachment& attachment : m_color_attachments)
    {
        // External textures belong to the caller, only destroy what this render target created.
        attachment.owned.Destroy();
        attachment.external = nullptr;
    }
    m_color_attachments.Clear();
    m_depth_stencil_attachment.owned.Destroy();
    m_depth_stencil_attachment.external = nullptr;
    m_depth_stencil_attachment.mip_level = 0;
    m_depth_stencil_attachment.layer = -1;
    m_use_depth_stencil = false;
}

Rndr::ErrorCode Rndr::Canvas::RenderTarget::ValidateAttachmentSizes() const
{
    i32 width = -1;
    i32 height = -1;
    i32 sample_count = -1;

    const auto check = [&](const Attachment& attachment)
    {
        const TextureDesc& texture_desc = attachment.Get().GetDesc();
        const i32 attachment_width = MipSize(texture_desc.width, attachment.mip_level);
        const i32 attachment_height = MipSize(texture_desc.height, attachment.mip_level);
        if (width == -1)
        {
            width = attachment_width;
            height = attachment_height;
            sample_count = texture_desc.sample_count;
            return ErrorCode::Success;
        }
        if (attachment_width != width || attachment_height != height)
        {
            RNDR_LOG_ERROR("Canvas: Attachments have different dimensions");
            return ErrorCode::InvalidArgument;
        }
        if (texture_desc.sample_count != sample_count)
        {
            RNDR_LOG_ERROR("Canvas: Attachments have different sample counts");
            return ErrorCode::InvalidArgument;
        }
        return ErrorCode::Success;
    };

    for (const Attachment& attachment : m_color_attachments)
    {
        RNDR_CANVAS_CHECK(check(attachment));
    }
    if (m_use_depth_stencil)
    {
        RNDR_CANVAS_CHECK(check(m_depth_stencil_attachment));
    }
    return ErrorCode::Success;
}

Rndr::i32 Rndr::Canvas::RenderTarget::GetColorAttachmentCount() const
{
    return static_cast<i32>(m_color_attachments.GetSize());
}

const Rndr::Canvas::Texture& Rndr::Canvas::RenderTarget::GetColorAttachment(i32 index) const
{
    return m_color_attachments[index].Get();
}

const Rndr::Canvas::Texture& Rndr::Canvas::RenderTarget::GetDepthStencilAttachment() const
{
    return m_depth_stencil_attachment.Get();
}

bool Rndr::Canvas::RenderTarget::IsColorAttachmentExternal(i32 index) const
{
    return m_color_attachments[index].IsExternal();
}

bool Rndr::Canvas::RenderTarget::IsDepthStencilExternal() const
{
    return m_depth_stencil_attachment.IsExternal();
}

Rndr::i32 Rndr::Canvas::RenderTarget::GetWidth() const
{
    if (!m_color_attachments.IsEmpty())
    {
        const Attachment& attachment = m_color_attachments[0];
        return MipSize(attachment.Get().GetDesc().width, attachment.mip_level);
    }
    if (m_depth_stencil_attachment.Get().IsValid())
    {
        return MipSize(m_depth_stencil_attachment.Get().GetDesc().width, m_depth_stencil_attachment.mip_level);
    }
    return -1;
}

Rndr::i32 Rndr::Canvas::RenderTarget::GetHeight() const
{
    if (!m_color_attachments.IsEmpty())
    {
        const Attachment& attachment = m_color_attachments[0];
        return MipSize(attachment.Get().GetDesc().height, attachment.mip_level);
    }
    if (m_depth_stencil_attachment.Get().IsValid())
    {
        return MipSize(m_depth_stencil_attachment.Get().GetDesc().height, m_depth_stencil_attachment.mip_level);
    }
    return -1;
}

Rndr::u32 Rndr::Canvas::RenderTarget::GetNativeHandle() const
{
    return m_handle;
}

const Opal::StringUtf8& Rndr::Canvas::RenderTarget::GetName() const
{
    return m_name;
}

bool Rndr::Canvas::RenderTarget::IsValid() const
{
    if (m_handle == 0)
    {
        return false;
    }
    if (m_color_attachments.IsEmpty() && !m_use_depth_stencil)
    {
        return false;
    }
    for (const Attachment& attachment : m_color_attachments)
    {
        if (!attachment.Get().IsValid())
        {
            return false;
        }
    }
    if (m_use_depth_stencil && !m_depth_stencil_attachment.Get().IsValid())
    {
        return false;
    }
    return true;
}
