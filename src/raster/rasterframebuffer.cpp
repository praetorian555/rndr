#include "rndr/raster/rasterframebuffer.h"

#if defined RNDR_RASTER

#include "rndr/raster/rastergraphicscontext.h"

rndr::FrameBuffer::FrameBuffer(GraphicsContext* Context, int Width, int Height, const FrameBufferProperties& Props)
    : m_Context(Context), m_Width(Width), m_Height(Height), m_Props(Props)
{
    assert(m_Context);
    assert(m_Props.ColorBufferCount > 0 && m_Props.ColorBufferCount <= PipelineConstants::MaxFrameBufferColorBuffers);

    bool bIsNull = Width == 0 || Height == 0;
    for (int i = 0; i < m_Props.ColorBufferCount; i++)
    {
        if (!bIsNull)
        {
            ImageProperties ImageProps;
            ImageProps.PixelLayout = m_Props.ColorPixelLayout[i];
            ImageProps.GammaSpace = m_Props.ColorGammaSpace[i];
            std::unique_ptr<Image> Image = std::make_unique<rndr::Image>(m_Width, m_Height, ImageProps);
            m_ColorBuffers.push_back(std::move(Image));
        }
        else
        {
            m_ColorBuffers.push_back(nullptr);
        }
    }

    m_DepthBuffer = nullptr;
    m_StencilBuffer = nullptr;
    if (m_Props.bUseDepthStencil && !bIsNull)
    {
        ImageProperties ImageProps;
        ImageProps.PixelLayout = m_Props.DepthPixelLayout;
        ImageProps.GammaSpace = GammaSpace::Linear;
        m_DepthBuffer = std::make_unique<rndr::Image>(m_Width, m_Height, ImageProps);

        ImageProps.PixelLayout = m_Props.StencilPixelLayout;
        ImageProps.GammaSpace = GammaSpace::Linear;
        m_StencilBuffer = std::make_unique<rndr::Image>(m_Width, m_Height, ImageProps);
    }
}

void rndr::FrameBuffer::SetSize(int Width, int Height)
{
    bool bIsNull = Width == 0 || Height == 0;

    m_Width = Width;
    m_Height = Height;
    for (int i = 0; i < m_Props.ColorBufferCount; i++)
    {
        m_ColorBuffers[i] = nullptr;
        if (!bIsNull)
        {
            ImageProperties ImageProps;
            ImageProps.PixelLayout = m_Props.ColorPixelLayout[i];
            ImageProps.GammaSpace = m_Props.ColorGammaSpace[i];
            m_ColorBuffers[i] = std::make_unique<rndr::Image>(m_Width, m_Height, ImageProps);
        }
    }

    m_DepthBuffer = nullptr;
    m_StencilBuffer = nullptr;
    if (m_Props.bUseDepthStencil && !bIsNull)
    {
        ImageProperties ImageProps;
        ImageProps.PixelLayout = m_Props.DepthPixelLayout;
        ImageProps.GammaSpace = GammaSpace::Linear;
        m_DepthBuffer = std::make_unique<rndr::Image>(m_Width, m_Height, ImageProps);

        ImageProps.PixelLayout = m_Props.StencilPixelLayout;
        ImageProps.GammaSpace = GammaSpace::Linear;
        m_StencilBuffer = std::make_unique<rndr::Image>(m_Width, m_Height, ImageProps);
    }
}

rndr::Image* rndr::FrameBuffer::GetColorBuffer(int Index)
{
    assert(Index >= 0 && Index < m_ColorBuffers.size());
    return m_ColorBuffers[Index].get();
}

rndr::Image* rndr::FrameBuffer::GetDepthBuffer()
{
    return m_DepthBuffer.get();
}

rndr::Image* rndr::FrameBuffer::GetStencilBuffer()
{
    return m_StencilBuffer.get();
}

bool rndr::FrameBuffer::IsUsingDepthStencil() const
{
    return m_Props.bUseDepthStencil;
}

int rndr::FrameBuffer::GetColorBufferCount() const
{
    return m_ColorBuffers.size();
}

int rndr::FrameBuffer::GetWidth() const
{
    return m_Width;
}

int rndr::FrameBuffer::GetHeight() const
{
    return m_Height;
}

rndr::GraphicsContext* rndr::FrameBuffer::GetContext() const
{
    return m_Context;
}

#endif  // RNDR_RASTER
