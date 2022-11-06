#include "rndr/render/dx11/dx11graphicscontext.h"

#if defined RNDR_DX11

#include <Windows.h>

#include "rndr/core/log.h"
#include "rndr/core/rndrcontext.h"

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
    m_DeviceContext->ClearState();
    m_DeviceContext->Flush();

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

rndr::SwapChain* rndr::GraphicsContext::CreateSwapChain(void* NativeWindowHandle, int Width, int Height, const SwapChainProperties& Props)
{
    SwapChain* S = RNDR_NEW(SwapChain, "rndr::GraphicsContext: SwapChain");
    if (!S || !S->Init(this, NativeWindowHandle, Width, Height, Props))
    {
        RNDR_DELETE(SwapChain, S);
    }
    return S;
}

rndr::Shader* rndr::GraphicsContext::CreateShader(const ByteSpan& ShaderContents, const ShaderProperties& Props)
{
    Shader* S = RNDR_NEW(Shader, "rndr::GraphicsContext: Shader");
    if (!S || !S->Init(this, ShaderContents, Props))
    {
        RNDR_DELETE(Shader, S);
    }
    return S;
}

rndr::Image* rndr::GraphicsContext::CreateImage(int Width, int Height, const ImageProperties& Props, ByteSpan InitData)
{
    Image* Im = RNDR_NEW(Image, "rndr::GraphicsContext: Image");
    if (!Im || !Im->Init(this, Width, Height, Props, InitData))
    {
        RNDR_DELETE(Image, Im);
    }
    return Im;
}

rndr::Image* rndr::GraphicsContext::CreateImageArray(int Width,
                                                     int Height,
                                                     int ArraySize,
                                                     const ImageProperties& Props,
                                                     Span<ByteSpan> InitData)
{
    Image* Im = RNDR_NEW(Image, "rndr::GraphicsContext: ImageArray");
    if (!Im || !Im->InitArray(this, Width, Height, ArraySize, Props, InitData))
    {
        RNDR_DELETE(Image, Im);
    }
    return Im;
}

rndr::Image* rndr::GraphicsContext::CreateCubeMap(int Width, int Height, const ImageProperties& Props, Span<ByteSpan> InitData)
{
    Image* Im = RNDR_NEW(Image, "rndr::GraphicsContext: CubeMap");
    if (!Im || !Im->InitCubeMap(this, Width, Height, Props, InitData))
    {
        RNDR_DELETE(Image, Im);
    }
    return Im;
}

rndr::Image* rndr::GraphicsContext::CreateImageForSwapChain(SwapChain* SwapChain, int BufferIndex)
{
    Image* Im = RNDR_NEW(Image, "rndr::GraphicsContext: ImageSwapChain");
    if (!Im || !Im->InitSwapchainBackBuffer(this, SwapChain, BufferIndex))
    {
        RNDR_DELETE(Image, Im);
    }
    return Im;
}

rndr::Sampler* rndr::GraphicsContext::CreateSampler(const SamplerProperties& Props)
{
    Sampler* S = RNDR_NEW(Sampler, "rndr::GraphicsContext: Sampler");
    if (!S || !S->Init(this, Props))
    {
        RNDR_DELETE(Sampler, S);
    }
    return S;
}

rndr::Buffer* rndr::GraphicsContext::CreateBuffer(const BufferProperties& Props, ByteSpan InitialData)
{
    Buffer* Buff = RNDR_NEW(Buffer, "rndr::GraphicsContext: Buffer");
    if (!Buff || !Buff->Init(this, Props, InitialData))
    {
        RNDR_DELETE(Buffer, Buff);
    }
    return Buff;
}

rndr::FrameBuffer* rndr::GraphicsContext::CreateFrameBuffer(int Width, int Height, const FrameBufferProperties& Props)
{
    FrameBuffer* FB = RNDR_NEW(FrameBuffer, "rndr::GraphicsContext: FrameBuffer");
    if (!FB || !FB->Init(this, Width, Height, Props))
    {
        RNDR_DELETE(FrameBuffer, FB);
    }
    return FB;
}

rndr::FrameBuffer* rndr::GraphicsContext::CreateFrameBufferForSwapChain(int Width, int Height, SwapChain* SwapChain)
{
    FrameBuffer* FB = RNDR_NEW(FrameBuffer, "rndr::GraphicsContext: FrameBuffer");
    if (!FB || !FB->InitForSwapChain(this, Width, Height, SwapChain))
    {
        RNDR_DELETE(FrameBuffer, FB);
    }
    return FB;
}

rndr::InputLayout* rndr::GraphicsContext::CreateInputLayout(Span<InputLayoutProperties> Props, Shader* Shader)
{
    InputLayout* Layout = RNDR_NEW(InputLayout, "rndr::GraphicsContext: InputLayout");
    if (!Layout || !Layout->Init(this, Props, Shader))
    {
        RNDR_DELETE(InputLayout, Layout);
    }
    return Layout;
}

rndr::RasterizerState* rndr::GraphicsContext::CreateRasterizerState(const RasterizerProperties& Props)
{
    RasterizerState* State = RNDR_NEW(RasterizerState, "rndr::GraphicsContext: RasterizerState");
    if (!State || !State->Init(this, Props))
    {
        RNDR_DELETE(RasterizerState, State);
    }
    return State;
}

rndr::DepthStencilState* rndr::GraphicsContext::CreateDepthStencilState(const DepthStencilProperties& Props)
{
    DepthStencilState* State = RNDR_NEW(DepthStencilState, "rndr::GraphicsContext: DepthStencilState");
    if (!State || !State->Init(this, Props))
    {
        RNDR_DELETE(DepthStencilState, State);
    }
    return State;
}

rndr::BlendState* rndr::GraphicsContext::CreateBlendState(const BlendProperties& Props)
{
    BlendState* State = RNDR_NEW(BlendState, "rndr::GraphicsContext: BlendState");
    if (!State || !State->Init(this, Props))
    {
        RNDR_DELETE(BlendState, State);
    }
    return State;
}

void rndr::GraphicsContext::ClearColor(Image* Image, math::Vector4 Color)
{
    if (!Image)
    {
        RNDR_LOG_ERROR("GraphicsContext::ClearColor: Invalid image!");
        return;
    }
    m_DeviceContext->ClearRenderTargetView(Image->DX11RenderTargetView, Color.Data);
    if (WindowsHasFailed())
    {
        const std::string ErrorMessage = WindowsGetErrorMessage();
        RNDR_LOG_ERROR("GraphicsContext::ClearColor: %s", ErrorMessage.c_str());
    }
}

void rndr::GraphicsContext::ClearDepth(Image* Image, real Depth)
{
    if (!Image)
    {
        RNDR_LOG_ERROR("GraphicsContext::ClearDepth: Invalid image!");
        return;
    }
    m_DeviceContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_DEPTH, Depth, 0);
}

void rndr::GraphicsContext::ClearStencil(Image* Image, uint8_t Stencil)
{
    if (!Image)
    {
        RNDR_LOG_ERROR("GraphicsContext::ClearStencil: Invalid image!");
        return;
    }
    m_DeviceContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_STENCIL, 0, Stencil);
}

void rndr::GraphicsContext::ClearDepthStencil(Image* Image, real Depth, uint8_t Stencil)
{
    if (!Image)
    {
        RNDR_LOG_ERROR("GraphicsContext::ClearDepthStencil: Invalid image!");
        return;
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
            m_DeviceContext->VSSetSamplers(Slot, 1, &Sampler->DX11State);
            break;
        }
        case ShaderType::Fragment:
        {
            m_DeviceContext->PSSetSamplers(Slot, 1, &Sampler->DX11State);
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

    if (Buffer->Props.Type == BufferType::Index)
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
        RNDR_LOG_ERROR("GraphicsContext::BindFrameBuffer: Invalid framebuffer!");
        return;
    }

    ID3D11DepthStencilView* DepthStencilView = FrameBuffer->DepthStencilBuffer->DX11DepthStencilView;
    std::vector<ID3D11RenderTargetView*> RenderTargetViews;
    const int RenderTargetCount = FrameBuffer->ColorBuffers.Size;
    RenderTargetViews.resize(RenderTargetCount);

    for (int i = 0; i < RenderTargetCount; i++)
    {
        RenderTargetViews[i] = FrameBuffer->ColorBuffers[i]->DX11RenderTargetView;
    }

    m_DeviceContext->OMSetRenderTargets(RenderTargetCount, RenderTargetViews.data(), DepthStencilView);
    m_DeviceContext->RSSetViewports(1, &FrameBuffer->DX11Viewport);
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

void rndr::GraphicsContext::Present(SwapChain* SwapChain, bool bVSync)
{
    // TODO: Look into ALLOW_TEARING
    const uint32_t Flags = 0;
    const HRESULT Result = SwapChain->DX11SwapChain->Present(bVSync, Flags);
    if (WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("GraphicsContext::Present: %s", ErrorMessage.c_str());
    }
}

#endif  // RNDR_DX11
