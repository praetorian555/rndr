#include "rndr/platform/opengl-frame-buffer.hpp"

#include "glad/glad.h"

#include "opengl-helpers.hpp"

#include "rndr/log.hpp"

Rndr::FrameBuffer::FrameBuffer(const Rndr::GraphicsContext& graphics_context, const Rndr::FrameBufferDesc& desc) : m_desc(desc)
{
    const ErrorCode err = Initialize(graphics_context, desc);
    if (err != ErrorCode::Success)
    {
        RNDR_LOG_ERROR("Failed to create frame buffer!");
    }
}

Rndr::ErrorCode Rndr::FrameBuffer::Initialize(const Rndr::GraphicsContext& graphics_context, const Rndr::FrameBufferDesc& desc)
{
    m_desc = desc;
    for (const Rndr::TextureDesc& color_attachment_desc : m_desc.color_attachments)
    {
        if (color_attachment_desc.type != Rndr::TextureType::Texture2D)
        {
            RNDR_LOG_ERROR("Color attachment must be of image type Texture2D!");
            return ErrorCode::InvalidArgument;
        }
    }
    if (m_desc.use_depth_stencil && m_desc.depth_stencil_attachment.type != Rndr::TextureType::Texture2D)
    {
        RNDR_LOG_ERROR("Depth stencil attachment must be of image type Texture2D!");
        return ErrorCode::InvalidArgument;
    }

    glCreateFramebuffers(1, &m_native_frame_buffer);
    if (m_native_frame_buffer == k_invalid_opengl_object)
    {
        RNDR_LOG_ERROR("Failed to create frame buffer!");
        return ErrorCode::OutOfMemory;
    }

    for (i32 i = 0; i < m_desc.color_attachments.GetSize(); ++i)
    {
        const TextureDesc& color_attachment_desc = m_desc.color_attachments[i];
        const SamplerDesc& sampler_desc = m_desc.color_attachment_samplers[i];
        Texture color_attachment(graphics_context, color_attachment_desc, sampler_desc);
        if (!color_attachment.IsValid())
        {
            RNDR_LOG_ERROR("Failed to create color attachment %d!", i);
            Destroy();
            return ErrorCode::InvalidArgument;
        }
        m_color_attachments.PushBack(Opal::Move(color_attachment));
        glNamedFramebufferTexture(m_native_frame_buffer, GL_COLOR_ATTACHMENT0 + i, m_color_attachments[i].GetNativeTexture(), 0);
        RNDR_GL_RETURN_ON_ERROR("Failed to attach color attachment to frame buffer!", Destroy());
    }

    if (m_desc.use_depth_stencil)
    {
        m_depth_stencil_attachment = Texture(graphics_context, m_desc.depth_stencil_attachment, m_desc.depth_stencil_sampler);
        if (!m_depth_stencil_attachment.IsValid())
        {
            RNDR_LOG_ERROR("Failed to create depth stencil attachment!");
            Destroy();
            return ErrorCode::InvalidArgument;
        }
        glNamedFramebufferTexture(m_native_frame_buffer, GL_DEPTH_STENCIL_ATTACHMENT, m_depth_stencil_attachment.GetNativeTexture(), 0);
        RNDR_GL_RETURN_ON_ERROR("Failed to attach depth stencil attachment to frame buffer!", Destroy());
    }

    const GLenum status = glCheckNamedFramebufferStatus(m_native_frame_buffer, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        RNDR_LOG_ERROR("Failed to create frame buffer!");
        Destroy();
        return ErrorCode::InvalidArgument;
    }
    return ErrorCode::Success;
}

Rndr::FrameBuffer::~FrameBuffer()
{
    Destroy();
}

Rndr::FrameBuffer::FrameBuffer(Rndr::FrameBuffer&& other) noexcept
    : m_desc(std::move(other.m_desc)),
      m_color_attachments(std::move(other.m_color_attachments)),
      m_depth_stencil_attachment(std::move(other.m_depth_stencil_attachment)),
      m_native_frame_buffer(other.m_native_frame_buffer)
{
    other.m_native_frame_buffer = k_invalid_opengl_object;
}

Rndr::FrameBuffer& Rndr::FrameBuffer::operator=(Rndr::FrameBuffer&& other) noexcept
{
    m_desc = other.m_desc;
    Destroy();
    m_color_attachments = std::move(other.m_color_attachments);
    m_depth_stencil_attachment = std::move(other.m_depth_stencil_attachment);
    m_native_frame_buffer = other.m_native_frame_buffer;
    other.m_native_frame_buffer = k_invalid_opengl_object;
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
    for (Texture& color_attachment : m_color_attachments)
    {
        color_attachment.Destroy();
    }
    m_color_attachments.Clear();
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
    for (const Texture& color_attachment : m_color_attachments)
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

Rndr::i32 Rndr::FrameBuffer::GetColorAttachmentCount() const
{
    return static_cast<i32>(m_color_attachments.GetSize());
}

const Rndr::Texture& Rndr::FrameBuffer::GetColorAttachment(i32 index) const
{
    RNDR_ASSERT(index >= 0 && index < m_color_attachments.GetSize(), "Color attachment index out of range!");
    return m_color_attachments[index];
}

const Rndr::Texture& Rndr::FrameBuffer::GetDepthStencilAttachment() const
{
    return m_depth_stencil_attachment;
}

GLuint Rndr::FrameBuffer::GetNativeFrameBuffer() const
{
    return m_native_frame_buffer;
}
