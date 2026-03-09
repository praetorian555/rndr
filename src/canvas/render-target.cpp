#include "rndr/canvas/render-target.hpp"

#include "glad/glad.h"

#include "rndr/exception.hpp"
#include "rndr/trace.hpp"

Rndr::Canvas::RenderTarget::RenderTarget(const Context& context, const RenderTargetDesc& desc, const Opal::StringUtf8& name)
    : m_use_depth_stencil(desc.use_depth_stencil), m_name(name.Clone())
{
    RNDR_CPU_EVENT_SCOPED("Canvas::RenderTarget::RenderTarget");

    if (desc.color_attachments.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "At least one color attachment is required!");
    }
    if (desc.color_attachments.GetSize() > k_max_color_attachments)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Too many color attachments!");
    }

    glCreateFramebuffers(1, &m_handle);
    if (m_handle == 0)
    {
        throw GraphicsAPIException(0, "Failed to create GL framebuffer!");
    }

    for (i32 i = 0; i < static_cast<i32>(desc.color_attachments.GetSize()); ++i)
    {
        const TextureDesc& tex_desc = desc.color_attachments[i];
        Texture color_tex(context, tex_desc);
        if (!color_tex.IsValid())
        {
            Destroy();
            throw GraphicsAPIException(0, "Failed to create color attachment!");
        }
        glNamedFramebufferTexture(m_handle, GL_COLOR_ATTACHMENT0 + i, color_tex.GetNativeHandle(), 0);
        m_color_attachments.PushBack(std::move(color_tex));
    }

    if (m_use_depth_stencil)
    {
        m_depth_stencil_attachment = Texture(context, desc.depth_stencil_attachment);
        if (!m_depth_stencil_attachment.IsValid())
        {
            Destroy();
            throw GraphicsAPIException(0, "Failed to create depth/stencil attachment!");
        }
        const GLenum depth_attachment = desc.depth_stencil_attachment.format == Format::D24S8 ? GL_DEPTH_STENCIL_ATTACHMENT
                                                                                                                      : GL_DEPTH_ATTACHMENT;
        glNamedFramebufferTexture(m_handle, depth_attachment, m_depth_stencil_attachment.GetNativeHandle(), 0);
    }

    const GLenum status = glCheckNamedFramebufferStatus(m_handle, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        Destroy();
        throw GraphicsAPIException(status, "Framebuffer is not complete!");
    }

    if (!m_name.IsEmpty())
    {
        glObjectLabel(GL_FRAMEBUFFER, m_handle, static_cast<GLsizei>(m_name.GetSize()), m_name.GetData());
    }
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

Rndr::Canvas::RenderTarget Rndr::Canvas::RenderTarget::Clone() const
{
    if (!IsValid())
    {
        return {};
    }

    RenderTarget clone;
    clone.m_use_depth_stencil = m_use_depth_stencil;
    clone.m_name = m_name.Clone();

    glCreateFramebuffers(1, &clone.m_handle);
    if (clone.m_handle == 0)
    {
        return {};
    }

    for (i32 i = 0; i < static_cast<i32>(m_color_attachments.GetSize()); ++i)
    {
        Texture cloned_tex = m_color_attachments[i].Clone();
        if (!cloned_tex.IsValid())
        {
            clone.Destroy();
            return {};
        }
        glNamedFramebufferTexture(clone.m_handle, GL_COLOR_ATTACHMENT0 + i, cloned_tex.GetNativeHandle(), 0);
        clone.m_color_attachments.PushBack(std::move(cloned_tex));
    }

    if (m_use_depth_stencil && m_depth_stencil_attachment.IsValid())
    {
        clone.m_depth_stencil_attachment = m_depth_stencil_attachment.Clone();
        if (!clone.m_depth_stencil_attachment.IsValid())
        {
            clone.Destroy();
            return {};
        }
        const GLenum depth_attachment = m_depth_stencil_attachment.GetDesc().format == Format::D24S8 ? GL_DEPTH_STENCIL_ATTACHMENT
                                                                                                                              : GL_DEPTH_ATTACHMENT;
        glNamedFramebufferTexture(clone.m_handle, depth_attachment, clone.m_depth_stencil_attachment.GetNativeHandle(), 0);
    }

    if (!clone.m_name.IsEmpty())
    {
        glObjectLabel(GL_FRAMEBUFFER, clone.m_handle, static_cast<GLsizei>(clone.m_name.GetSize()), clone.m_name.GetData());
    }

    return clone;
}

void Rndr::Canvas::RenderTarget::Destroy()
{
    if (m_handle != 0)
    {
        glDeleteFramebuffers(1, &m_handle);
        m_handle = 0;
    }
    for (Texture& tex : m_color_attachments)
    {
        tex.Destroy();
    }
    m_color_attachments.Clear();
    m_depth_stencil_attachment.Destroy();
    m_use_depth_stencil = false;
}

Rndr::i32 Rndr::Canvas::RenderTarget::GetColorAttachmentCount() const
{
    return static_cast<i32>(m_color_attachments.GetSize());
}

const Rndr::Canvas::Texture& Rndr::Canvas::RenderTarget::GetColorAttachment(i32 index) const
{
    return m_color_attachments[index];
}

const Rndr::Canvas::Texture& Rndr::Canvas::RenderTarget::GetDepthStencilAttachment() const
{
    return m_depth_stencil_attachment;
}

Rndr::i32 Rndr::Canvas::RenderTarget::GetWidth() const
{
    if (!m_color_attachments.IsEmpty())
    {
        return m_color_attachments[0].GetDesc().width;
    }
    if (m_depth_stencil_attachment.IsValid())
    {
        return m_depth_stencil_attachment.GetDesc().width;
    }
    return -1;
}

Rndr::i32 Rndr::Canvas::RenderTarget::GetHeight() const
{
    if (!m_color_attachments.IsEmpty())
    {
        return m_color_attachments[0].GetDesc().height;
    }
    if (m_depth_stencil_attachment.IsValid())
    {
        return m_depth_stencil_attachment.GetDesc().height;
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
    if (m_color_attachments.IsEmpty())
    {
        return false;
    }
    for (const Texture& tex : m_color_attachments)
    {
        if (!tex.IsValid())
        {
            return false;
        }
    }
    if (m_use_depth_stencil && !m_depth_stencil_attachment.IsValid())
    {
        return false;
    }
    return true;
}
