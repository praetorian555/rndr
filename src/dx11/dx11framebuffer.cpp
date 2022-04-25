#include "rndr/dx11/dx11framebuffer.h"

#if defined RNDR_DX11

#include "rndr/core/log.h"

#include "rndr/dx11/dx11graphicscontext.h"
#include "rndr/dx11/dx11helpers.h"

rndr::FrameBuffer::FrameBuffer(GraphicsContext* Context, int Width, int Height, const FrameBufferProperties& Props)
    : m_GraphicsContext(Context), m_Width(Width), m_Height(Height), m_Props(Props)
{
    if (m_Props.bWindowFrameBuffer)
    {
        RNDR_LOG_INFO("Creating window framebuffer...");

        IDXGISwapChain* Swapchain = m_GraphicsContext->GetSwapchain();
        ID3D11Texture2D* BackBuffer;
        HRESULT Result = Swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&BackBuffer);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR("Failed to get back buffer from the swapchain!");
            return;
        }

        std::unique_ptr<Image> I = std::make_unique<Image>(m_GraphicsContext, m_Width, m_Height, Props.ColorBufferProperties[0]);
        m_ColorBuffers.push_back(std::move(I));
    }

    if (m_Props.ColorBufferCount > 1)
    {
        const int StartIndex = m_Props.bWindowFrameBuffer ? 1 : 0;
        for (int i = StartIndex; i < m_Props.ColorBufferCount; i++)
        {
            std::unique_ptr<Image> I = std::make_unique<Image>(m_GraphicsContext, m_Width, m_Height, Props.ColorBufferProperties[i]);
            m_ColorBuffers.push_back(std::move(I));
        }
    }

    if (Props.bUseDepthStencil)
    {
        m_DepthStencilBuffer = std::make_unique<Image>(m_GraphicsContext, m_Width, m_Height, Props.DepthStencilBufferProperties);
    }
}

void rndr::FrameBuffer::SetSize(int Width, int Height)
{
    m_Width = Width;
    m_Height = Height;

    m_ColorBuffers.clear();
    m_DepthStencilBuffer.reset(nullptr);

    IDXGISwapChain* Swapchain = m_GraphicsContext->GetSwapchain();
    DXGI_FORMAT NewFormat = FromPixelFormat(m_Props.ColorBufferProperties[0].PixelFormat);
    Swapchain->ResizeBuffers(1, Width, Height, NewFormat, 0);

    if (m_Props.bWindowFrameBuffer)
    {
        RNDR_LOG_INFO("Creating window framebuffer...");

        IDXGISwapChain* Swapchain = m_GraphicsContext->GetSwapchain();
        ID3D11Texture2D* BackBuffer;
        HRESULT Result = Swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&BackBuffer);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR("Failed to get back buffer from the swapchain!");
            return;
        }

        std::unique_ptr<Image> I = std::make_unique<Image>(m_GraphicsContext, m_Width, m_Height, m_Props.ColorBufferProperties[0]);
        m_ColorBuffers.push_back(std::move(I));
    }

    if (m_Props.ColorBufferCount > 1)
    {
        const int StartIndex = m_Props.bWindowFrameBuffer ? 1 : 0;
        for (int i = StartIndex; i < m_Props.ColorBufferCount; i++)
        {
            std::unique_ptr<Image> I = std::make_unique<Image>(m_GraphicsContext, m_Width, m_Height, m_Props.ColorBufferProperties[i]);
            m_ColorBuffers.push_back(std::move(I));
        }
    }

    if (m_Props.bUseDepthStencil)
    {
        m_DepthStencilBuffer = std::make_unique<Image>(m_GraphicsContext, m_Width, m_Height, m_Props.DepthStencilBufferProperties);
    }
}

rndr::Image* rndr::FrameBuffer::GetColorBuffer(int Index)
{
    assert(Index > 0 && Index < m_Props.ColorBufferCount);
    return m_ColorBuffers[Index].get();
}

rndr::Image* rndr::FrameBuffer::GetDepthStencilBuffer()
{
    return m_DepthStencilBuffer.get();
}

bool rndr::FrameBuffer::IsUsingDepthStencil() const
{
    return m_Props.bUseDepthStencil;
}

int rndr::FrameBuffer::GetColorBufferCount() const
{
    return m_Props.ColorBufferCount;
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
    return m_GraphicsContext;
}

#endif  // RNDR_DX11
