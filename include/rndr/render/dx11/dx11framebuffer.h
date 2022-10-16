#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <vector>

#include <d3d11.h>

#include "rndr/render/graphicstypes.h"
#include "rndr/render/image.h"

namespace rndr
{

class GraphicsContext;
struct SwapChain;

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

    bool Init(rndr::GraphicsContext* Context, int Width, int Height, const FrameBufferProperties& Props = FrameBufferProperties{});
    bool InitForSwapChain(rndr::GraphicsContext* Context,
                          rndr::SwapChain* SwapChain,
                          int Width,
                          int Height,
                          const FrameBufferProperties& Props = FrameBufferProperties{});

    void SetSize(int Width, int Height);

    void Clear();
};

}  // namespace rndr

#endif  // RNDR_DX11
