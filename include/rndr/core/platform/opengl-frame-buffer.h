#pragma once

#include "rndr/core/containers/array.h"
#include "rndr/core/graphics-types.h"
#include "rndr/core/platform/opengl-image.h"

namespace Rndr
{

class GraphicsContext;

class FrameBuffer
{
public:
    FrameBuffer() = default;
    FrameBuffer(const GraphicsContext& graphics_context, const FrameBufferDesc& desc);
    ~FrameBuffer();

    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;
    FrameBuffer(FrameBuffer&& other) noexcept;
    FrameBuffer& operator=(FrameBuffer&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const FrameBufferDesc& GetDesc() const;
    [[nodiscard]] int32_t GetColorAttachmentCount() const;
    [[nodiscard]] const Image& GetColorAttachment(int32_t index) const;
    [[nodiscard]] const Image& GetDepthStencilAttachment() const;
    [[nodiscard]] GLuint GetNativeFrameBuffer() const;

private:
    FrameBufferDesc m_desc;
    Array<Image> m_color_attachments;
    Image m_depth_stencil_attachment;
    GLuint m_native_frame_buffer = k_invalid_opengl_object;
};

}  // namespace Rndr
