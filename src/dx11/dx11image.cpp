#include "rndr/dx11/dx11image.h"

#if defined RNDR_DX11

#include <Windows.h>
#include <d3d11.h>

#include "rndr/core/graphicscontext.h"
#include "rndr/core/log.h"

#include "rndr/dx11/dx11helpers.h"

rndr::Image::Image(GraphicsContext* Context, int Width, int Height, const ImageProperties& Props)
    : m_GraphicsContext(Context), m_Width(Width), m_Height(Height), m_Props(Props), m_bSwapchainBackBuffer(false)
{
    Create();
}

rndr::Image::Image(GraphicsContext* Context) : m_GraphicsContext(Context), m_bSwapchainBackBuffer(true)
{
    IDXGISwapChain* Swapchain = m_GraphicsContext->GetSwapchain();
    DXGI_SWAP_CHAIN_DESC Desc;
    Swapchain->GetDesc(&Desc);
    m_Width = Desc.BufferDesc.Width;
    m_Height = Desc.BufferDesc.Height;
    m_Props.PixelFormat = ToPixelFormat(Desc.BufferDesc.Format);
    m_Props.CPUAccess = CPUAccess::None;
    m_Props.Usage = Usage::GPUReadWrite;
    m_Props.bUseMips = false;

    Create();
}

void rndr::Image::Create()
{
    ID3D11Device* Device = m_GraphicsContext->GetDevice();
    HRESULT Result;

    if (!m_bSwapchainBackBuffer)
    {
        D3D11_TEXTURE2D_DESC Desc;
        ZeroMemory(&Desc, sizeof(D3D11_TEXTURE2D_DESC));
        Desc.ArraySize = 1;
        Desc.BindFlags = IsRenderTarget(m_Props.PixelFormat) ? D3D11_BIND_RENDER_TARGET : D3D11_BIND_DEPTH_STENCIL;
        Desc.CPUAccessFlags = FromCPUAccess(m_Props.CPUAccess);
        Desc.Format = FromPixelFormat(m_Props.PixelFormat);
        Desc.Width = m_Width;
        Desc.Height = m_Height;
        Desc.MiscFlags = 0;
        Desc.Usage = FromUsage(m_Props.Usage);
        Desc.SampleDesc.Count = 1;  // TODO(mkostic): Add options for multisampling in props
        Desc.SampleDesc.Quality = 0;

        Result = Device->CreateTexture2D(&Desc, nullptr, &m_Texture);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR("Failed to create ID3D11Texture2D!");
            return;
        }
    }
    else
    {
        IDXGISwapChain* Swapchain = m_GraphicsContext->GetSwapchain();
        HRESULT Result = Swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&m_Texture);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR("Failed to get back buffer from the swapchain!");
            return;
        }
    }

    if (IsRenderTarget(m_Props.PixelFormat))
    {
        Result = Device->CreateRenderTargetView(m_Texture, nullptr, &m_RenderTargetView);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR("Failed to create ID3D11RenderTargetView!");
        }
    }
    else
    {
        Result = Device->CreateDepthStencilView(m_Texture, nullptr, &m_DepthStencilView);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR("Failed to create ID3D11DepthStencilView!");
        }
    }
}

rndr::Image::~Image()
{
    DX11SafeRelease(m_Texture);
    if (IsRenderTarget(m_Props.PixelFormat))
    {
        DX11SafeRelease(m_RenderTargetView);
    }
    else
    {
        DX11SafeRelease(m_DepthStencilView);
    }
}

ID3D11RenderTargetView* rndr::Image::GetRenderTargetView()
{
    return m_RenderTargetView;
}

ID3D11DepthStencilView* rndr::Image::GetStencilTargetView()
{
    return m_DepthStencilView;
}

#endif  // RNDR_DX11
