#pragma once

#include "rndr/core/base.h"
#include "rndr/core/memory.h"

#if RNDR_DX11

#include "rndr/render/graphicstypes.h"
#include "rndr/render/framebuffer.h"

struct IDXGISwapChain;

namespace rndr
{

class GraphicsContext;

template <typename T>
class ScopePtr;

struct SwapChain
{
    SwapChainProperties Props;
    int Width = 0, Height = 0;

    IDXGISwapChain* DX11SwapChain = nullptr;
    ScopePtr<FrameBuffer> FrameBuffer;

    SwapChain() = default;
    ~SwapChain();

    SwapChain(const SwapChain& Other) = delete;
    SwapChain& operator=(const SwapChain& Other) = delete;

    SwapChain(SwapChain&& Other) = delete;
    SwapChain& operator=(SwapChain&& Other) = delete;

    bool Init(GraphicsContext* Context,
              NativeWindowHandle Handle,
              int InWidth,
              int InHeight,
              const SwapChainProperties& InProps = SwapChainProperties{});
};

}  // namespace rndr

#endif
