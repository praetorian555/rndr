#include "rndr/render/dx11/dx11framebuffer.h"

#if defined RNDR_DX11

#include "math/point2.h"

#include "rndr/core/log.h"

#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/dx11/dx11image.h"

rndr::FrameBuffer::~FrameBuffer()
{
    Clear();
}

bool rndr::FrameBuffer::Init(GraphicsContext* Context, int Width, int Height, const FrameBufferProperties& Props)
{
    if (Width == 0 || Height == 0)
    {
        RNDR_LOG_ERROR("FrameBuffer::Init: Invalid width or height!");
        return false;
    }
    if (Props.ColorBufferCount <= 0 || Props.ColorBufferCount > GraphicsConstants::MaxFrameBufferColorBuffers)
    {
        RNDR_LOG_ERROR("FrameBuffer::Init: Invalid number of color buffers!");
        return false;
    }

    this->Props = Props;
    this->Width = Width;
    this->Height = Height;

    return InitInternal(Context);
}

bool rndr::FrameBuffer::Resize(rndr::GraphicsContext* Context, int Width, int Height)
{
    if (Width == 0 || Height == 0)
    {
        RNDR_LOG_ERROR("FrameBuffer::Resize: Invalid width or height!");
        return false;
    }

    Clear();

    return InitInternal(Context);
}

bool rndr::FrameBuffer::UpdateViewport(float Width, float Height, const math::Point2& TopLeft, float MinDepth, float MaxDepth)
{
    if (Width == 0 || Height == 0)
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

    DX11Viewport.Width = Width;
    DX11Viewport.Height = Height;
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
            delete ColorBuffers[i];
        }
        delete[] ColorBuffers.Data;
        ColorBuffers.Data = nullptr;
        ColorBuffers.Size = 0;
    }
    if (DepthStencilBuffer)
    {
        delete DepthStencilBuffer;
        DepthStencilBuffer = nullptr;
    }
}

bool rndr::FrameBuffer::InitInternal(GraphicsContext* Context)
{
    ColorBuffers = Span<Image*>(new Image*[Props.ColorBufferCount], Props.ColorBufferCount);
    for (int i = 0; i < Props.ColorBufferCount; i++)
    {
        ColorBuffers[i] = nullptr;
    }
    DepthStencilBuffer = nullptr;

    ByteSpan EmptyData;
    for (int i = 0; i < Props.ColorBufferCount; i++)
    {
        ColorBuffers[i] = Context->CreateImage(Width, Height, Props.ColorBufferProperties[i], EmptyData);
        if (!ColorBuffers[i])
        {
            return false;
        }
    }
    if (Props.bUseDepthStencil)
    {
        DepthStencilBuffer = Context->CreateImage(Width, Height, Props.DepthStencilBufferProperties, EmptyData);
        if (!DepthStencilBuffer)
        {
            return false;
        }
    }

    DX11Viewport.Width = Width;
    DX11Viewport.Height = Height;
    DX11Viewport.TopLeftX = 0;
    DX11Viewport.TopLeftY = 0;
    DX11Viewport.MinDepth = 0.0;
    DX11Viewport.MaxDepth = 1.0;

    return true;
}

#endif  // RNDR_DX11
