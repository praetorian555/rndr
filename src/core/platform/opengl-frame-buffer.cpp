#include "rndr/core/platform/opengl-frame-buffer.h"

#include <glad/glad.h>

#include "core/platform/opengl-helpers.h"

Rndr::FrameBuffer::FrameBuffer(const Rndr::GraphicsContext& graphics_context, const Rndr::FrameBufferDesc& desc) : m_desc(desc)
{
    if (m_desc.color_attachments.GetSize() == 0)
    {
        RNDR_LOG_ERROR("Frame buffer must have at least one color attachment!");
        return;
    }
    for (const Rndr::ImageDesc& color_attachment_desc : m_desc.color_attachments)
    {
        if (color_attachment_desc.type != Rndr::ImageType::Image2D)
        {
            RNDR_LOG_ERROR("Color attachment must be of image type Image2D!");
            return;
        }
    }
    if (m_desc.use_depth_stencil && m_desc.depth_stencil_attachment.type != Rndr::ImageType::Image2D)
    {
        RNDR_LOG_ERROR("Depth stencil attachment must be of image type Image2D!");
        return;
    }

    glCreateFramebuffers(1, &m_native_frame_buffer);
    RNDR_ASSERT_OPENGL();

    for (int32_t i = 0; i < m_desc.color_attachments.GetSize(); ++i)
    {
        const Rndr::ImageDesc& color_attachment_desc = m_desc.color_attachments[i];
        Image color_attachment(graphics_context, color_attachment_desc);
        if (!color_attachment.IsValid())
        {
            RNDR_LOG_ERROR("Failed to create color attachment %d!", i);
            return;
        }
        m_color_attachments.PushBack(Opal::Move(color_attachment));
        glNamedFramebufferTexture(m_native_frame_buffer, GL_COLOR_ATTACHMENT0 + i, m_color_attachments[i].GetNativeTexture(), 0);
        RNDR_ASSERT_OPENGL();
    }

    if (m_desc.use_depth_stencil)
    {
        m_depth_stencil_attachment = Image(graphics_context, m_desc.depth_stencil_attachment);
        if (!m_depth_stencil_attachment.IsValid())
        {
            RNDR_LOG_ERROR("Failed to create depth stencil attachment!");
            return;
        }
        glNamedFramebufferTexture(m_native_frame_buffer, GL_DEPTH_STENCIL_ATTACHMENT, m_depth_stencil_attachment.GetNativeTexture(), 0);
        RNDR_ASSERT_OPENGL();
    }

    const GLenum status = glCheckNamedFramebufferStatus(m_native_frame_buffer, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        RNDR_HALT("Failed to create frame buffer!");
    }
}

Rndr::FrameBuffer::~FrameBuffer()
{
    Destroy();
}

Rndr::FrameBuffer::FrameBuffer(Rndr::FrameBuffer&& other) noexcept
    : m_desc(std::move(other.m_desc)),
      m_color_attachments(std::move(other.m_color_attachments)),
      m_depth_stencil_attachment(std::move(other.m_depth_stencil_attachment))
{
}

Rndr::FrameBuffer& Rndr::FrameBuffer::operator=(Rndr::FrameBuffer&& other) noexcept
{
    m_desc = other.m_desc;
    m_color_attachments = std::move(other.m_color_attachments);
    m_depth_stencil_attachment = std::move(other.m_depth_stencil_attachment);
    return *this;
}

void Rndr::FrameBuffer::Destroy()
{
    if (m_native_frame_buffer != k_invalid_opengl_object)
    {
        glDeleteFramebuffers(1, &m_native_frame_buffer);
        RNDR_ASSERT_OPENGL();
        m_native_frame_buffer = k_invalid_opengl_object;
    }
    for (Image& color_attachment : m_color_attachments)
    {
        color_attachment.Destroy();
    }
    m_depth_stencil_attachment.Destroy();
}

bool Rndr::FrameBuffer::IsValid() const
{
    if (m_native_frame_buffer == k_invalid_opengl_object)
    {
        return false;
    }
    if (m_color_attachments.IsEmpty())
    {
        return false;
    }
    for (const Image& color_attachment : m_color_attachments)
    {
        if (!color_attachment.IsValid())
        {
            return false;
        }
    }
    if (m_desc.use_depth_stencil && !m_depth_stencil_attachment.IsValid())
    {
        return false;
    }
    return true;
}

const Rndr::FrameBufferDesc& Rndr::FrameBuffer::GetDesc() const
{
    return m_desc;
}

int32_t Rndr::FrameBuffer::GetColorAttachmentCount() const
{
    return static_cast<int32_t>(m_color_attachments.GetSize());
}

const Rndr::Image& Rndr::FrameBuffer::GetColorAttachment(int32_t index) const
{
    RNDR_ASSERT(index >= 0 && index < m_color_attachments.GetSize());
    return m_color_attachments[index];
}

const Rndr::Image& Rndr::FrameBuffer::GetDepthStencilAttachment() const
{
    return m_depth_stencil_attachment;
}

GLuint Rndr::FrameBuffer::GetNativeFrameBuffer() const
{
    return m_native_frame_buffer;
}
