#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <vector>

#include <d3d11.h>

#include "rndr/render/graphicstypes.h"
#include "rndr/render/image.h"

class math::Point2;

namespace rndr
{

class GraphicsContext;
struct SwapChain;

struct FrameBuffer
{
    int Width, Height;
    FrameBufferProperties Props;

    class RndrContext* Context = nullptr;
    Span<Image*> ColorBuffers;
    Image* DepthStencilBuffer = nullptr;

    D3D11_VIEWPORT DX11Viewport;

    FrameBuffer() = default;
    ~FrameBuffer();

    bool Init(rndr::GraphicsContext* Context, int Width, int Height, const FrameBufferProperties& Props = FrameBufferProperties{});
    bool InitForSwapChain(rndr::GraphicsContext* Context, int Width, int Height, rndr::SwapChain* SwapChain);

    bool Resize(rndr::GraphicsContext* Context, int Width, int Height);
    bool UpdateViewport(float Width, float Height, const math::Point2& TopLeft, float MinDepth, float MaxDepth);

private:
    void Clear();
    bool InitInternal(rndr::GraphicsContext* Context, rndr::SwapChain* SwapChain = nullptr);
};

}  // namespace rndr

#endif  // RNDR_DX11
