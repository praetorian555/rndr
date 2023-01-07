#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <vector>

#include <d3d11.h>

#include "rndr/render/graphicstypes.h"
#include "rndr/render/image.h"

namespace math
{
class Point2;
}

namespace rndr
{

class GraphicsContext;
struct SwapChain;

struct FrameBuffer
{
    int Width, Height;
    FrameBufferProperties Props;

    Span<ScopePtr<Image>> ColorBuffers;
    ScopePtr<Image> DepthStencilBuffer;

    D3D11_VIEWPORT DX11Viewport;

    FrameBuffer() = default;
    ~FrameBuffer();

    bool Init(rndr::GraphicsContext* Context,
              int InWidth,
              int InHeight,
              const FrameBufferProperties& InProps = FrameBufferProperties{});
    bool Init(rndr::GraphicsContext* Context,
              int InWidth,
              int InHeight,
              rndr::SwapChain* SwapChain);

    bool Resize(rndr::GraphicsContext* Context, int InWidth, int InHeight);
    bool UpdateViewport(float InWidth,
                        float InHeight,
                        const math::Point2& TopLeft,
                        float MinDepth,
                        float MaxDepth);

private:
    void Clear();
    bool InitInternal(rndr::GraphicsContext* Context, rndr::SwapChain* SwapChain = nullptr);
};

}  // namespace rndr

#endif  // RNDR_DX11
