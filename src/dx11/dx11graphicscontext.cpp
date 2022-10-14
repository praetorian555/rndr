#include "rndr/render/dx11/dx11graphicscontext.h"

#if defined RNDR_DX11

#include <Windows.h>

#include "rndr/core/log.h"
#include "rndr/core/window.h"

#include "rndr/render/dx11/dx11buffer.h"
#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/dx11/dx11image.h"
#include "rndr/render/dx11/dx11pipeline.h"
#include "rndr/render/dx11/dx11sampler.h"
#include "rndr/render/dx11/dx11shader.h"
#include "rndr/render/framebuffer.h"

rndr::GraphicsContext::GraphicsContext(Window* Window, GraphicsContextProperties Props) : m_Window(Window), m_Props(Props)
{

    HWND WindowHandle = reinterpret_cast<HWND>(m_Window->GetNativeWindowHandle());

    DXGI_SWAP_CHAIN_DESC SwapChainDesc;
    ZeroMemory(&SwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
    SwapChainDesc.BufferCount = 1;
    SwapChainDesc.BufferDesc.Width = m_Window->GetWidth();
    SwapChainDesc.BufferDesc.Height = m_Window->GetHeight();
    SwapChainDesc.BufferDesc.Format = DX11FromPixelFormat(m_Props.FrameBuffer.ColorBufferProperties[0].PixelFormat);
    SwapChainDesc.BufferDesc.RefreshRate = DXGI_RATIONAL{0, 1};  // TODO(mkostic): Figure this out
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.OutputWindow = WindowHandle;
    // If you want no multisamling use count=1 and quality=0
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDesc.Windowed = TRUE;
    UINT CreateDeviceFlags = 0;
#if RNDR_DEBUG
    CreateDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif
    // These are the feature levels that we will accept.
    D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
                                         D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1};
    // This will be the feature level that is used to create our device and swap chain.
    IDXGIAdapter* Adapter = nullptr;  // Use default adapter
    HMODULE SoftwareRasterizerModule = nullptr;
    HRESULT Result = D3D11CreateDeviceAndSwapChain(Adapter, D3D_DRIVER_TYPE_HARDWARE, SoftwareRasterizerModule, CreateDeviceFlags,
                                                   FeatureLevels, _countof(FeatureLevels), D3D11_SDK_VERSION, &SwapChainDesc, &m_Swapchain,
                                                   &m_Device, &m_FeatureLevel, &m_DeviceContext);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to create DX11 device and swapchain!");
        return;
    }

    m_WindowFrameBuffer = std::make_unique<FrameBuffer>(this, m_Window->GetWidth(), m_Window->GetHeight(), m_Props.FrameBuffer);

    WindowDelegates::OnResize.Add(RNDR_BIND_THREE_PARAM(this, &GraphicsContext::WindowResize));
}

rndr::GraphicsContext::~GraphicsContext()
{
    DX11SafeRelease(m_Swapchain);
    DX11SafeRelease(m_Device);
    DX11SafeRelease(m_DeviceContext);
}

void rndr::GraphicsContext::WindowResize(Window* Window, int Width, int Height)
{
    if (Window != m_Window)
    {
        return;
    }

    m_WindowFrameBuffer->SetSize(Width, Height);
}

ID3D11Device* rndr::GraphicsContext::GetDevice()
{
    return m_Device;
}

ID3D11DeviceContext* rndr::GraphicsContext::GetDeviceContext()
{
    return m_DeviceContext;
}

IDXGISwapChain* rndr::GraphicsContext::GetSwapchain()
{
    return m_Swapchain;
}

D3D_FEATURE_LEVEL rndr::GraphicsContext::GetFeatureLevel()
{
    return m_FeatureLevel;
}

rndr::Shader* rndr::GraphicsContext::CreateShader(const ShaderProperties& Props)
{
    return new Shader(this, Props);
}

rndr::Image* rndr::GraphicsContext::CreateImage(int Width, int Height, const ImageProperties& Props, ByteSpan InitData)
{
    Image* Im = new Image();
    Im->Init(this, Width, Height, Props, InitData);
    return Im;
}

rndr::Image* rndr::GraphicsContext::CreateImageArray(int Width, int Height, const ImageProperties& Props, Span<ByteSpan> InitData)
{
    Image* Im = new Image();
    Im->InitArray(this, Width, Height, Props, InitData);
    return Im;
}

rndr::Image* rndr::GraphicsContext::CreateImageForSwapchainBackBuffer()
{
    Image* Im = new Image();
    Im->InitSwapchainBackBuffer(this);
    return Im;
}

rndr::Sampler* rndr::GraphicsContext::CreateSampler(const SamplerProperties& Props)
{
    return new Sampler(this, Props);
}

rndr::Buffer* rndr::GraphicsContext::CreateBuffer(const BufferProperties& Props, ByteSpan InitialData)
{
    return new Buffer(this, Props, InitialData);
}

rndr::FrameBuffer* rndr::GraphicsContext::CreateFrameBuffer(int Width, int Height, const FrameBufferProperties& Props)
{
    return new FrameBuffer(this, Width, Height, Props);
}

rndr::InputLayout* rndr::GraphicsContext::CreateInputLayout(Span<InputLayoutProperties> Props, Shader* Shader)
{
    return new InputLayout(this, Props, Shader);
}

rndr::RasterizerState* rndr::GraphicsContext::CreateRasterizerState(const RasterizerProperties& Props)
{
    return new RasterizerState(this, Props);
}

rndr::DepthStencilState* rndr::GraphicsContext::CreateDepthStencilState(const DepthStencilProperties& Props)
{
    return new DepthStencilState(this, Props);
}

rndr::BlendState* rndr::GraphicsContext::CreateBlendState(const BlendProperties& Props)
{
    return new BlendState(this, Props);
}

void rndr::GraphicsContext::ClearColor(Image* Image, math::Vector4 Color)
{
    if (!Image)
    {
        Image = m_WindowFrameBuffer->ColorBuffers[0];
    }
    m_DeviceContext->ClearRenderTargetView(Image->DX11RenderTargetView, Color.Data);
}

void rndr::GraphicsContext::ClearDepth(Image* Image, real Depth)
{
    if (!Image)
    {
        Image = m_WindowFrameBuffer->DepthStencilBuffer;
    }
    m_DeviceContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_DEPTH, Depth, 0);
}

void rndr::GraphicsContext::ClearStencil(Image* Image, uint8_t Stencil)
{
    if (!Image)
    {
        Image = m_WindowFrameBuffer->DepthStencilBuffer;
    }
    m_DeviceContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_STENCIL, 0, Stencil);
}

void rndr::GraphicsContext::ClearDepthStencil(Image* Image, real Depth, uint8_t Stencil)
{
    if (!Image)
    {
        Image = m_WindowFrameBuffer->DepthStencilBuffer;
    }
    m_DeviceContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, Depth, Stencil);
}

void rndr::GraphicsContext::BindShader(Shader* Shader)
{
    switch (Shader->Props.Type)
    {
        case ShaderType::Vertex:
        {
            m_DeviceContext->VSSetShader(Shader->DX11VertexShader, nullptr, 0);
            break;
        }
        case ShaderType::Fragment:
        {
            m_DeviceContext->PSSetShader(Shader->DX11FragmentShader, nullptr, 0);
            break;
        }
        default:
        {
            assert(false);
        }
    }
}

void rndr::GraphicsContext::BindImageAsShaderResource(Image* Image, int Slot, Shader* Shader)
{
    assert(Shader);
    assert(Image);

    switch (Shader->Props.Type)
    {
        case ShaderType::Vertex:
        {
            m_DeviceContext->VSSetShaderResources(Slot, 1, &Image->DX11ShaderResourceView);
            break;
        }
        case ShaderType::Fragment:
        {
            m_DeviceContext->PSSetShaderResources(Slot, 1, &Image->DX11ShaderResourceView);
            break;
        }
        default:
        {
            assert(false);
        }
    }
}

void rndr::GraphicsContext::BindSampler(Sampler* Sampler, int Slot, Shader* Shader)
{
    assert(Shader);
    assert(Sampler);

    switch (Shader->Props.Type)
    {
        case ShaderType::Vertex:
        {
            m_DeviceContext->VSSetSamplers(Slot, 1, &Sampler->m_State);
            break;
        }
        case ShaderType::Fragment:
        {
            m_DeviceContext->PSSetSamplers(Slot, 1, &Sampler->m_State);
            break;
        }
        default:
        {
            assert(false);
        }
    }
}

void rndr::GraphicsContext::BindBuffer(Buffer* Buffer, int Slot, Shader* Shader)
{
    if (Shader)
    {
        switch (Shader->Props.Type)
        {
            case ShaderType::Vertex:
            {
                m_DeviceContext->VSSetConstantBuffers(Slot, 1, &Buffer->DX11Buffer);
                break;
            }
            case ShaderType::Fragment:
            {
                m_DeviceContext->PSSetConstantBuffers(Slot, 1, &Buffer->DX11Buffer);
                break;
            }
            default:
            {
                assert(false);
            }
        }
        return;
    }

    if (Buffer->Props.BindFlag == BufferBindFlag::Index)
    {
        assert(Buffer->Props.Stride == 4 || Buffer->Props.Stride == 2);
        DXGI_FORMAT Format = Buffer->Props.Stride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
        m_DeviceContext->IASetIndexBuffer(Buffer->DX11Buffer, Format, 0);
    }
    else
    {
        const uint32_t Stride = Buffer->Props.Stride;
        const uint32_t Offset = 0;
        m_DeviceContext->IASetVertexBuffers(Slot, 1, &Buffer->DX11Buffer, &Stride, &Offset);
    }
}

void rndr::GraphicsContext::BindFrameBuffer(FrameBuffer* FrameBuffer)
{
    if (!FrameBuffer)
    {
        FrameBuffer = m_WindowFrameBuffer.get();
    }
    assert(FrameBuffer);

    ID3D11DepthStencilView* DepthStencilView = FrameBuffer->DepthStencilBuffer->DX11DepthStencilView;
    std::vector<ID3D11RenderTargetView*> RenderTargetViews;
    const int RenderTargetCount = FrameBuffer->ColorBuffers.Size;
    RenderTargetViews.resize(RenderTargetCount);

    for (int i = 0; i < RenderTargetCount; i++)
    {
        RenderTargetViews[i] = FrameBuffer->ColorBuffers[i]->DX11RenderTargetView;
    }

    m_DeviceContext->OMSetRenderTargets(RenderTargetCount, RenderTargetViews.data(), DepthStencilView);
    m_DeviceContext->RSSetViewports(1, &FrameBuffer->Viewport);
}

void rndr::GraphicsContext::BindInputLayout(InputLayout* InputLayout)
{
    m_DeviceContext->IASetInputLayout(InputLayout->DX11InputLayout);
}

void rndr::GraphicsContext::BindRasterizerState(RasterizerState* State)
{
    m_DeviceContext->RSSetState(State->DX11RasterizerState);
}

void rndr::GraphicsContext::BindDepthStencilState(DepthStencilState* State)
{
    m_DeviceContext->OMSetDepthStencilState(State->DX11DepthStencilState, State->Props.StencilRefValue);
}

void rndr::GraphicsContext::BindBlendState(BlendState* State)
{
    m_DeviceContext->OMSetBlendState(State->DX11BlendState, nullptr, 0xFFFFFFFF);
}

void rndr::GraphicsContext::DrawIndexed(PrimitiveTopology Topology, int IndicesCount)
{
    m_DeviceContext->IASetPrimitiveTopology(DX11FromPrimitiveTopology(Topology));
    m_DeviceContext->DrawIndexed(IndicesCount, 0, 0);
}

void rndr::GraphicsContext::DrawIndexedInstanced(PrimitiveTopology Topology,
                                                 int IndexCount,
                                                 int InstanceCount,
                                                 int IndexOffset,
                                                 int InstanceOffset)
{
    m_DeviceContext->IASetPrimitiveTopology(DX11FromPrimitiveTopology(Topology));
    m_DeviceContext->DrawIndexedInstanced(IndexCount, InstanceCount, IndexOffset, 0, InstanceOffset);
}

void rndr::GraphicsContext::Present(bool bVSync)
{
    const uint32_t SyncInterval = bVSync ? 1 : 0;
    const uint32_t Flags = 0;
    m_Swapchain->Present(SyncInterval, Flags);
}

void rndr::GraphicsContext::DestroyShader(Shader* Shader)
{
    delete Shader;
}

void rndr::GraphicsContext::DestroyImage(Image* Image)
{
    delete Image;
}

void rndr::GraphicsContext::DestroySampler(Sampler* Sampler)
{
    delete Sampler;
}

void rndr::GraphicsContext::DestroyBuffer(Buffer* Buffer)
{
    delete Buffer;
}

void rndr::GraphicsContext::DestroyFrameBuffer(FrameBuffer* FrameBuffer)
{
    delete FrameBuffer;
}

void rndr::GraphicsContext::DestroyInputLayout(InputLayout* InputLayout)
{
    delete InputLayout;
}

void rndr::GraphicsContext::DestroyRasterizerState(RasterizerState* State)
{
    delete State;
}

void rndr::GraphicsContext::DestroyDepthStencilState(DepthStencilState* State)
{
    delete State;
}

void rndr::GraphicsContext::DestroyBlendState(BlendState* State)
{
    delete State;
}

rndr::FrameBuffer* rndr::GraphicsContext::GetWindowFrameBuffer()
{
    return m_WindowFrameBuffer.get();
}

#endif  // RNDR_DX11
