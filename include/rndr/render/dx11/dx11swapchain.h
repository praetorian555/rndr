#pragma once

#include "rndr/core/base.h"

#if RNDR_DX11

#include "rndr/render/graphicstypes.h"

struct IDXGISwapChain;

namespace rndr
{

class GraphicsContext;
struct FrameBuffer;

struct SwapChain
{
    SwapChainProperties Props;
    int Width = 0, Height = 0;

    IDXGISwapChain* DX11SwapChain = nullptr;
    FrameBuffer* FrameBuffer = nullptr;

    SwapChain() = default;
    ~SwapChain();

    bool Init(GraphicsContext* Context,
              NativeWindowHandle Handle,
              int InWidth,
              int InHeight,
              const SwapChainProperties& InProps = SwapChainProperties{});
};

}  // namespace rndr

#endif
