#include "rndr/render/dx11/dx11framebuffer.h"

#if defined RNDR_DX11

#include "math/point2.h"

#include "rndr/core/log.h"
#include "rndr/core/memory.h"

#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/dx11/dx11image.h"
#include "rndr/render/dx11/dx11swapchain.h"

rndr::FrameBuffer::~FrameBuffer()
{
    Clear();
}

bool rndr::FrameBuffer::Init(GraphicsContext* Context,
                             int InWidth,
                             int InHeight,
                             const FrameBufferProperties& InProps)
{
    if (InWidth == 0 || InHeight == 0)
    {
        RNDR_LOG_ERROR("FrameBuffer::Init: Invalid width or height!");
        return false;
    }
    if (InProps.ColorBufferCount <= 0 ||
        InProps.ColorBufferCount > GraphicsConstants::MaxFrameBufferColorBuffers)
    {
        RNDR_LOG_ERROR("FrameBuffer::Init: Invalid number of color buffers!");
        return false;
    }

    Props = InProps;
    Width = InWidth;
    Height = InHeight;

    return InitInternal(Context);
}

bool rndr::FrameBuffer::InitForSwapChain(rndr::GraphicsContext* Context,
                                         int InWidth,
                                         int InHeight,
                                         rndr::SwapChain* SwapChain)
{
    if (!Context)
    {
        RNDR_LOG_ERROR("FrameBuffer::InitForSwapChain: Invalid graphics context!");
        return false;
    }
    if (!SwapChain)
    {
        RNDR_LOG_ERROR("FrameBuffer::InitForSwapChain: Invalid swapchain!");
        return false;
    }

    Props.ColorBufferCount = 1;
    Props.ColorBufferProperties[0].bUseMips = false;
    Props.ColorBufferProperties[0].SampleCount = 1;
    Props.ColorBufferProperties[0].PixelFormat = SwapChain->Props.ColorFormat;
    Props.ColorBufferProperties[0].Usage = Usage::Default;
    Props.ColorBufferProperties[0].ImageBindFlags = ImageBindFlags::RenderTarget;
    Props.bUseDepthStencil = SwapChain->Props.bUseDepthStencil;
    Props.DepthStencilBufferProperties.bUseMips = false;
    Props.DepthStencilBufferProperties.SampleCount = 1;
    Props.DepthStencilBufferProperties.PixelFormat = SwapChain->Props.DepthStencilFormat;
    Props.DepthStencilBufferProperties.Usage = Usage::Default;
    Props.DepthStencilBufferProperties.ImageBindFlags = ImageBindFlags::DepthStencil;
    Width = InWidth;
    Height = InHeight;

    if (Width == 0 || Height == 0)
    {
        RNDR_LOG_ERROR("FrameBuffer::InitForSwapChain: Invalid width or height!");
        return false;
    }
    if (Props.ColorBufferCount <= 0 ||
        Props.ColorBufferCount > GraphicsConstants::MaxFrameBufferColorBuffers)
    {
        RNDR_LOG_ERROR("FrameBuffer::InitForSwapChain: Invalid number of color buffers!");
        return false;
    }

    return InitInternal(Context, SwapChain);
}

bool rndr::FrameBuffer::Resize(rndr::GraphicsContext* Context, int InWidth, int InHeight)
{
    if (InWidth == 0 || InHeight == 0)
    {
        RNDR_LOG_ERROR("FrameBuffer::Resize: Invalid width or height!");
        return false;
    }

    Clear();

    Width = InWidth;
    Height = InHeight;

    return InitInternal(Context);
}

bool rndr::FrameBuffer::UpdateViewport(float InWidth,
                                       float InHeight,
                                       const math::Point2& TopLeft,
                                       float MinDepth,
                                       float MaxDepth)
{
    if (InWidth == 0 || InHeight == 0)
    {
        RNDR_LOG_ERROR("FrameBuffer::UpdateViewport: Invalid width or height!");
        return false;
    }
    if (MinDepth > MaxDepth)
    {
        RNDR_LOG_ERROR("FrameBuffer::UpdateViewport: Minimal depth is larger then maximal depth!");
        return false;
    }
    MinDepth = math::Clamp(MinDepth, 0.0f, 1.0f);
    MaxDepth = math::Clamp(MaxDepth, 0.0f, 1.0f);

    DX11Viewport.Width = InWidth;
    DX11Viewport.Height = InHeight;
    DX11Viewport.TopLeftX = TopLeft.X;
    DX11Viewport.TopLeftY = TopLeft.Y;
    DX11Viewport.MinDepth = MinDepth;
    DX11Viewport.MaxDepth = MaxDepth;

    return true;
}

void rndr::FrameBuffer::Clear()
{
    if (ColorBuffers)
    {
        for (int i = 0; i < ColorBuffers.Size; i++)
        {
            RNDR_DELETE(Image, ColorBuffers[i]);
        }
        RNDR_DELETE(Image*, ColorBuffers.Data);
        ColorBuffers.Data = nullptr;
        ColorBuffers.Size = 0;
    }
    if (DepthStencilBuffer)
    {
        RNDR_DELETE(Image, DepthStencilBuffer);
    }
}

bool rndr::FrameBuffer::InitInternal(GraphicsContext* Context, SwapChain* SwapChain)
{
    ColorBuffers.Data = RNDR_NEW_ARRAY(Image*, Props.ColorBufferCount, "rndr::FrameBuffer: Image");
    ColorBuffers.Size = Props.ColorBufferCount;
    for (int i = 0; i < Props.ColorBufferCount; i++)
    {
        ColorBuffers[i] = nullptr;
    }
    DepthStencilBuffer = nullptr;

    ByteSpan EmptyData;
    for (int i = 0; i < Props.ColorBufferCount; i++)
    {
        if (!SwapChain)
        {
            ColorBuffers[i] =
                Context->CreateImage(Width, Height, Props.ColorBufferProperties[i], EmptyData);
        }
        else
        {
            ColorBuffers[i] = Context->CreateImageForSwapChain(SwapChain, i);
        }
        if (!ColorBuffers[i])
        {
            RNDR_LOG_ERROR("FrameBuffer::InitInternal: Failed to create color image!");
            return false;
        }
    }
    if (Props.bUseDepthStencil)
    {
        DepthStencilBuffer =
            Context->CreateImage(Width, Height, Props.DepthStencilBufferProperties, EmptyData);
        if (!DepthStencilBuffer)
        {
            RNDR_LOG_ERROR("FrameBuffer::InitInternal: Failed to create depth stencil image!");
            return false;
        }
    }

    DX11Viewport.Width = static_cast<float>(Width);
    DX11Viewport.Height = static_cast<float>(Height);
    DX11Viewport.TopLeftX = 0;
    DX11Viewport.TopLeftY = 0;
    DX11Viewport.MinDepth = 0.0;
    DX11Viewport.MaxDepth = 1.0;

    return true;
}

#endif  // RNDR_DX11
