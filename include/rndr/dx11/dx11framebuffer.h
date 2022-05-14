#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <vector>

#include <d3d11.h>

#include "rndr/core/graphicstypes.h"
#include "rndr/core/image.h"

namespace rndr
{

class GraphicsContext;

struct FrameBuffer
{
    GraphicsContext* GraphicsContext;
    int Width, Height;
    FrameBufferProperties Props;
    Span<Image*> ColorBuffers;
    Image* DepthStencilBuffer = nullptr;
    D3D11_VIEWPORT Viewport;

    FrameBuffer(rndr::GraphicsContext* Context, int Width, int Height, const FrameBufferProperties& Props = FrameBufferProperties{});
    ~FrameBuffer();

    void SetSize(int Width, int Height);

    void Clear();
};

}  // namespace rndr

#endif  // RNDR_DX11
