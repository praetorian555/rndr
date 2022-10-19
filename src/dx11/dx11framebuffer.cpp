#include "rndr/render/dx11/dx11framebuffer.h"

#if defined RNDR_DX11

#include "rndr/core/log.h"

#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/dx11/dx11image.h"

rndr::FrameBuffer::FrameBuffer(rndr::GraphicsContext* Context, int Width, int Height, const FrameBufferProperties& Props)
    : GraphicsContext(Context), Width(Width), Height(Height), Props(Props)
{
    ColorBuffers = Span<Image*>(new Image*[Props.ColorBufferCount], Props.ColorBufferCount);
    for (int i = 0; i < Props.ColorBufferCount; i++)
    {
        ColorBuffers[i] = nullptr;
    }
    DepthStencilBuffer = nullptr;

    if (Props.bWindowFrameBuffer)
    {
        RNDR_LOG_INFO("Creating window framebuffer...");

        // Special image that uses swapchain buffer
        ColorBuffers[0] = GraphicsContext->CreateImageForSwapchainBackBuffer();
    }

    if (Props.ColorBufferCount > 1)
    {
        const int StartIndex = Props.bWindowFrameBuffer ? 1 : 0;
        for (int i = StartIndex; i < Props.ColorBufferCount; i++)
        {
            ColorBuffers[i] = GraphicsContext->CreateImage(Width, Height, Props.ColorBufferProperties[i], ByteSpan{});
        }
    }

    if (Props.bUseDepthStencil)
    {
        DepthStencilBuffer = GraphicsContext->CreateImage(Width, Height, Props.DepthStencilBufferProperties, ByteSpan{});
    }

    Viewport.Width = Width;
    Viewport.Height = Height;
    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.MinDepth = 0.0;
    Viewport.MaxDepth = 1.0;
}

rndr::FrameBuffer::~FrameBuffer()
{
    for (int i = 0; i < ColorBuffers.Size; i++)
    {
        if (ColorBuffers[i])
        {
            GraphicsContext->DestroyImage(ColorBuffers[i]);
            ColorBuffers[i] = nullptr;
        }
    }
    delete[] ColorBuffers.Data;

    if (DepthStencilBuffer)
    {
        GraphicsContext->DestroyImage(DepthStencilBuffer);
        DepthStencilBuffer = nullptr;
    }
}

void rndr::FrameBuffer::SetSize(int W, int H)
{
    Width = W;
    Height = H;

    for (int i = 0; i < ColorBuffers.Size; i++)
    {
        if (ColorBuffers[i])
        {
            GraphicsContext->DestroyImage(ColorBuffers[i]);
            ColorBuffers[i] = nullptr;
        }
    }
    delete[] ColorBuffers.Data;

    if (DepthStencilBuffer)
    {
        GraphicsContext->DestroyImage(DepthStencilBuffer);
        DepthStencilBuffer = nullptr;
    }

    ColorBuffers = Span<Image*>(new Image*[Props.ColorBufferCount], Props.ColorBufferCount);
    for (int i = 0; i < Props.ColorBufferCount; i++)
    {
        ColorBuffers[i] = nullptr;
    }
    DepthStencilBuffer = nullptr;

    // IDXGISwapChain* Swapchain = GraphicsContext->GetSwapchain();
    // DXGI_FORMAT NewFormat = DX11FromPixelFormat(Props.ColorBufferProperties[0].PixelFormat);
    // Swapchain->ResizeBuffers(1, Width, Height, NewFormat, 0);

    if (Props.bWindowFrameBuffer)
    {
        RNDR_LOG_INFO("Resizing window framebuffer...");

        // Special image that uses swapchain buffer
        ColorBuffers[0] = GraphicsContext->CreateImageForSwapchainBackBuffer();
    }

    if (Props.ColorBufferCount > 1)
    {
        const int StartIndex = Props.bWindowFrameBuffer ? 1 : 0;
        for (int i = StartIndex; i < Props.ColorBufferCount; i++)
        {
            ColorBuffers[i] = GraphicsContext->CreateImage(Width, Height, Props.ColorBufferProperties[i], ByteSpan{});
        }
    }

    if (Props.bUseDepthStencil)
    {
        DepthStencilBuffer = GraphicsContext->CreateImage(Width, Height, Props.DepthStencilBufferProperties, ByteSpan{});
    }

    Viewport.Width = Width;
    Viewport.Height = Height;
    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.MinDepth = 0.0;
    Viewport.MaxDepth = 1.0;
}

#endif  // RNDR_DX11
