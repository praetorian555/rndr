#include "rndr/render/dx11/dx11graphicscontext.h"

#if defined RNDR_DX11

#include <Windows.h>

#include "rndr/core/log.h"
#include "rndr/core/window.h"

#include "rndr/render/dx11/dx11buffer.h"
#include "rndr/render/dx11/dx11framebuffer.h"
#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/dx11/dx11image.h"
#include "rndr/render/dx11/dx11pipeline.h"
#include "rndr/render/dx11/dx11sampler.h"
#include "rndr/render/dx11/dx11shader.h"
#include "rndr/render/dx11/dx11swapchain.h"

std::string rndr::GraphicsContext::WindowsGetErrorMessage(HRESULT ErrorCode)
{
    std::string Rtn;
    if (ErrorCode != S_OK)
    {
        constexpr DWORD BufferSize = 1024;
        char Buffer[BufferSize] = {};
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, ErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), Buffer, BufferSize,
                      nullptr);
        Rtn += Buffer;
    }
    else
    {
        Rtn += "Error occurred in debug layer.\n";
    }

    if (!m_DebugInfoQueue)
    {
        return Rtn;
    }

    bool bAddNewLine = false;
    UINT64 MessageCount = m_DebugInfoQueue->GetNumStoredMessages();
    for (UINT64 i = 0; i < MessageCount; i++)
    {
        SIZE_T MessageSize = 0;
        m_DebugInfoQueue->GetMessage(i, nullptr, &MessageSize);
        D3D11_MESSAGE* Message = (D3D11_MESSAGE*)malloc(MessageSize);
        HRESULT Result = m_DebugInfoQueue->GetMessage(i, Message, &MessageSize);
        assert(!FAILED(Result));
        if (!Message)
        {
            continue;
        }
        bool bShouldLog = Message->Severity == D3D11_MESSAGE_SEVERITY_ERROR;
        bShouldLog |= m_Props.bFailWarning && Message->Severity == D3D11_MESSAGE_SEVERITY_WARNING;
        if (bShouldLog)
        {
            bAddNewLine = true;
            Rtn += "\n\t";
            Rtn += Message->pDescription;
        }

        free(Message);
    }

    if (bAddNewLine)
    {
        Rtn += "\n";
    }

    m_DebugInfoQueue->ClearStoredMessages();

    return Rtn;
}

bool rndr::GraphicsContext::WindowsHasFailed(HRESULT ErrorCode)
{
    if (FAILED(ErrorCode))
    {
        return true;
    }
    if (!m_DebugInfoQueue)
    {
        return false;
    }

    bool bStatus = false;
    UINT64 MessageCount = m_DebugInfoQueue->GetNumStoredMessages();
    for (UINT64 i = 0; i < MessageCount; i++)
    {
        SIZE_T MessageSize = 0;
        m_DebugInfoQueue->GetMessage(i, nullptr, &MessageSize);
        D3D11_MESSAGE* Message = (D3D11_MESSAGE*)malloc(MessageSize);
        HRESULT Result = m_DebugInfoQueue->GetMessage(i, Message, &MessageSize);
        assert(!FAILED(Result));
        if (!Message)
        {
            continue;
        }

        bool bShouldFail = Message->Severity == D3D11_MESSAGE_SEVERITY_ERROR;
        bShouldFail |= m_Props.bFailWarning && Message->Severity == D3D11_MESSAGE_SEVERITY_WARNING;
        if (bShouldFail)
        {
            bStatus = true;
        }

        free(Message);
    }

    return bStatus;
}

rndr::GraphicsContext::~GraphicsContext()
{
    DX11SafeRelease(m_Device);
    DX11SafeRelease(m_DeviceContext);
    if (m_DebugInfoQueue)
    {
        DX11SafeRelease(m_DebugInfoQueue);
    }
}

bool rndr::GraphicsContext::Init(GraphicsContextProperties Props)
{
    m_Props = Props;

    UINT Flags = 0;
#if RNDR_DEBUG
    if (m_Props.bEnableDebugLayer)
    {
        Flags |= D3D11_CREATE_DEVICE_DEBUG;
    }
#endif
    if (m_Props.bDisableGPUTimeout)
    {
        Flags |= D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT;
    }
    if (!m_Props.bMakeThreadSafe)
    {
        Flags |= D3D11_CREATE_DEVICE_SINGLETHREADED;
    }
    // These are the feature levels that we will accept.
    D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
                                         D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1};
    // This will be the feature level that is used to create our device and swap chain.
    IDXGIAdapter* Adapter = nullptr;  // Use default adapter
    HMODULE SoftwareRasterizerModule = nullptr;
    HRESULT Result = D3D11CreateDevice(Adapter, D3D_DRIVER_TYPE_HARDWARE, SoftwareRasterizerModule, Flags, FeatureLevels,
                                       _countof(FeatureLevels), D3D11_SDK_VERSION, &m_Device, &m_FeatureLevel, &m_DeviceContext);
    if (WindowsHasFailed(Result))
    {
        std::string ErrorMessage = WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    if (!m_Props.bEnableDebugLayer)
    {
        return true;
    }

    Result = m_Device->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&m_DebugInfoQueue);
    if (WindowsHasFailed(Result))
    {
        std::string ErrorMessage = WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

ID3D11Device* rndr::GraphicsContext::GetDevice()
{
    return m_Device;
}

ID3D11DeviceContext* rndr::GraphicsContext::GetDeviceContext()
{
    return m_DeviceContext;
}

D3D_FEATURE_LEVEL rndr::GraphicsContext::GetFeatureLevel()
{
    return m_FeatureLevel;
}

rndr::SwapChain* rndr::GraphicsContext::CreateSwapChain(const SwapChainProperties& Props)
{
    return nullptr;
}

rndr::Shader* rndr::GraphicsContext::CreateShader(const ShaderProperties& Props)
{
    return new Shader(this, Props);
}

rndr::Image* rndr::GraphicsContext::CreateImage(int Width, int Height, const ImageProperties& Props, ByteSpan InitData)
{
    Image* Im = new Image();
    bool Status = Im->Init(this, Width, Height, Props, InitData);
    if (!Status)
    {
        delete Im;
        Im = nullptr;
    }
    return Im;
}

rndr::Image* rndr::GraphicsContext::CreateImageArray(int Width,
                                                     int Height,
                                                     int ArraySize,
                                                     const ImageProperties& Props,
                                                     Span<ByteSpan> InitData)
{
    Image* Im = new Image();
    bool Status = Im->InitArray(this, Width, Height, ArraySize, Props, InitData);
    if (!Status)
    {
        delete Im;
        Im = nullptr;
    }
    return Im;
}

rndr::Image* rndr::GraphicsContext::CreateCubeMap(int Width, int Height, const ImageProperties& Props, Span<ByteSpan> InitData)
{
    Image* Im = new Image();
    bool Status = Im->InitCubeMap(this, Width, Height, Props, InitData);
    if (!Status)
    {
        delete Im;
        Im = nullptr;
    }
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

rndr::FrameBuffer* rndr::GraphicsContext::CreateFrameBufferForSwapChain(SwapChain* SwapChain,
                                                                        int Width,
                                                                        int Height,
                                                                        const FrameBufferProperties& Props)
{
    return nullptr;
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
        // Image = m_WindowFrameBuffer->ColorBuffers[0];
    }
    m_DeviceContext->ClearRenderTargetView(Image->DX11RenderTargetView, Color.Data);
}

void rndr::GraphicsContext::ClearDepth(Image* Image, real Depth)
{
    if (!Image)
    {
        // Image = m_WindowFrameBuffer->DepthStencilBuffer;
    }
    m_DeviceContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_DEPTH, Depth, 0);
}

void rndr::GraphicsContext::ClearStencil(Image* Image, uint8_t Stencil)
{
    if (!Image)
    {
        // Image = m_WindowFrameBuffer->DepthStencilBuffer;
    }
    m_DeviceContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_STENCIL, 0, Stencil);
}

void rndr::GraphicsContext::ClearDepthStencil(Image* Image, real Depth, uint8_t Stencil)
{
    if (!Image)
    {
        // Image = m_WindowFrameBuffer->DepthStencilBuffer;
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
        // FrameBuffer = m_WindowFrameBuffer.get();
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
    // m_Swapchain->Present(SyncInterval, Flags);
}

void rndr::GraphicsContext::DestroySwapChain(SwapChain* SwapChain) {}

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

#endif  // RNDR_DX11
