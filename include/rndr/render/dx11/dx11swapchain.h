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
    IDXGISwapChain* DX11SwapChain;
    SwapChainProperties Props;
    FrameBuffer* FrameBuffer;

    bool Init(GraphicsContext* Context, void* NativeWindowHandle, const SwapChainProperties& Props = SwapChainProperties{});
};

}  // namespace rndr

#endif
