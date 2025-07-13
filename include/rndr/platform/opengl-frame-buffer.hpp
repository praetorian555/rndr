#pragma once

#include "opal/container/dynamic-array.h"

#include "rndr/error-codes.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/platform/opengl-texture.hpp"

namespace Rndr
{

class GraphicsContext;

class FrameBuffer
{
public:
    FrameBuffer() = default;
    FrameBuffer(const GraphicsContext& graphics_context, const FrameBufferDesc& desc);

    ErrorCode Initialize(const GraphicsContext& graphics_context, const FrameBufferDesc& desc);

    ~FrameBuffer();
    void Destroy();

    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;
    FrameBuffer(FrameBuffer&& other) noexcept;
    FrameBuffer& operator=(FrameBuffer&& other) noexcept;

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const FrameBufferDesc& GetDesc() const;
    [[nodiscard]] i32 GetColorAttachmentCount() const;
    [[nodiscard]] const Texture& GetColorAttachment(i32 index) const;
    [[nodiscard]] const Texture& GetDepthStencilAttachment() const;
    [[nodiscard]] GLuint GetNativeFrameBuffer() const;

private:
    FrameBufferDesc m_desc;
    Opal::DynamicArray<Texture> m_color_attachments;
    Texture m_depth_stencil_attachment;
    GLuint m_native_frame_buffer = k_invalid_opengl_object;
};

}  // namespace Rndr
