#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <vector>

#include "rndr/core/graphicstypes.h"
#include "rndr/core/image.h"

namespace rndr
{

class GraphicsContext;

class FrameBuffer
{
public:
    FrameBuffer(GraphicsContext* Context, int Width, int Height, const FrameBufferProperties& Props = FrameBufferProperties{});

    void SetSize(int Width, int Height);

    Image* GetColorBuffer(int Index = 0);
    Image* GetDepthStencilBuffer();

    bool IsUsingDepthStencil() const;
    int GetColorBufferCount() const;
    int GetWidth() const;
    int GetHeight() const;
    GraphicsContext* GetContext() const;

private:
    GraphicsContext* m_GraphicsContext;

    int m_Width, m_Height;
    FrameBufferProperties m_Props;

    std::vector<std::unique_ptr<Image>> m_ColorBuffers;
    std::unique_ptr<Image> m_DepthStencilBuffer;
};

}  // namespace rndr

#endif  // RNDR_DX11
