#include "rndr/render/dx11/dx11graphicscontext.h"

#if defined RNDR_DX11

#include <array>

#include <Windows.h>

#include "rndr/core/log.h"
#include "rndr/core/memory.h"
#include "rndr/core/rndrcontext.h"

#include "rndr/render/dx11/dx11buffer.h"
#include "rndr/render/dx11/dx11commandlist.h"
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

#if RNDR_DEBUG
    if (ErrorCode != S_OK)
    {
        constexpr DWORD kBufferSize = 1024;
        std::array<char, kBufferSize> Buffer{};
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, ErrorCode,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), Buffer.data(), kBufferSize,
                      nullptr);
        Rtn += Buffer.data();
    }
    else
    {
        Rtn += "Error occurred in debug layer.\n";
    }

    if (m_DebugInfoQueue == nullptr)
    {
        return Rtn;
    }

    bool AddNewLine = false;
    const uint64_t MessageCount = m_DebugInfoQueue->GetNumStoredMessages();
    for (uint64_t MsgIndex = 0; MsgIndex < MessageCount; MsgIndex++)
    {
        SIZE_T MessageSize = 0;
        m_DebugInfoQueue->GetMessage(MsgIndex, nullptr, &MessageSize);
        D3D11_MESSAGE* Message = static_cast<D3D11_MESSAGE*>(malloc(MessageSize));
        HRESULT const Result = m_DebugInfoQueue->GetMessage(MsgIndex, Message, &MessageSize);
        assert(!FAILED(Result));
        if (Message == nullptr)
        {
            continue;
        }
        bool BShouldLog = Message->Severity == D3D11_MESSAGE_SEVERITY_ERROR;
        BShouldLog |=
            m_Props.ShouldFailWarning && Message->Severity == D3D11_MESSAGE_SEVERITY_WARNING;
        if (BShouldLog)
        {
            AddNewLine = true;
            Rtn += "\n\t";
            Rtn += Message->pDescription;
        }

        free(Message);
    }

    if (AddNewLine)
    {
        Rtn += "\n";
    }

    m_DebugInfoQueue->ClearStoredMessages();
#endif  // RNDR_DEBUG

    return Rtn;
}

bool rndr::GraphicsContext::WindowsHasFailed(HRESULT ErrorCode)
{
    if (FAILED(ErrorCode))
    {
        return true;
    }

#if RNDR_DEBUG
    if (m_DebugInfoQueue == nullptr)
    {
        return false;
    }

    bool BStatus = false;
    UINT64 const MessageCount = m_DebugInfoQueue->GetNumStoredMessages();
    for (UINT64 I = 0; I < MessageCount; I++)
    {
        SIZE_T MessageSize = 0;
        m_DebugInfoQueue->GetMessage(I, nullptr, &MessageSize);
        D3D11_MESSAGE* Message = static_cast<D3D11_MESSAGE*>(malloc(MessageSize));
        HRESULT const Result = m_DebugInfoQueue->GetMessage(I, Message, &MessageSize);
        assert(!FAILED(Result));
        if (Message == nullptr)
        {
            continue;
        }

        bool BShouldFail = Message->Severity == D3D11_MESSAGE_SEVERITY_ERROR;
        BShouldFail |=
            m_Props.ShouldFailWarning && Message->Severity == D3D11_MESSAGE_SEVERITY_WARNING;
        if (BShouldFail)
        {
            BStatus = true;
        }

        free(Message);
    }

    return BStatus;
#else
    return false;
#endif  // RNDR_DEBUG
}

rndr::GraphicsContext::~GraphicsContext()
{
    m_DeviceContext->ClearState();
    m_DeviceContext->Flush();

    DX11SafeRelease(m_Device);
    DX11SafeRelease(m_DeviceContext);

#if RNDR_DEBUG
    if (m_DebugInfoQueue != nullptr)
    {
        DX11SafeRelease(m_DebugInfoQueue);
    }
#endif  // RNDR_DEBUG
}

bool rndr::GraphicsContext::Init(GraphicsContextProperties Props)
{
    m_Props = Props;

    UINT Flags = 0;
#if RNDR_DEBUG
    if (m_Props.EnableDebugLayer)
    {
        Flags |= D3D11_CREATE_DEVICE_DEBUG;
    }
#endif
    if (m_Props.DisableGpuTimeout)
    {
        Flags |= D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT;
    }
    if (!m_Props.IsThreadSafe)
    {
        Flags |= D3D11_CREATE_DEVICE_SINGLETHREADED;
    }
    // These are the feature levels that we will accept.
    D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                         D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
                                         D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
                                         D3D_FEATURE_LEVEL_9_1};
    // This will be the feature level that is used to create our device and swap chain.
    IDXGIAdapter* Adapter = nullptr;  // Use default adapter
    HMODULE SoftwareRasterizerModule = nullptr;
    HRESULT Result = D3D11CreateDevice(
        Adapter, D3D_DRIVER_TYPE_HARDWARE, SoftwareRasterizerModule, Flags, FeatureLevels,
        _countof(FeatureLevels), D3D11_SDK_VERSION, &m_Device, &m_FeatureLevel, &m_DeviceContext);
    if (WindowsHasFailed(Result))
    {
        std::string const ErrorMessage = WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    if (!m_Props.EnableDebugLayer)
    {
        return true;
    }

#if RNDR_DEBUG
    Result = m_Device->QueryInterface(__uuidof(ID3D11InfoQueue),
                                      reinterpret_cast<void**>(&m_DebugInfoQueue));
    if (WindowsHasFailed(Result))
    {
        std::string const ErrorMessage = WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }
#endif  // RNDR_DEBUG

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

template <typename GraphicsObjectType, typename... Args>
static rndr::ScopePtr<GraphicsObjectType> CreateGraphicsObject(std::string_view MemoryTag,
                                                               Args&&... Arguments)
{
    rndr::ScopePtr<GraphicsObjectType> Object = rndr::CreateScoped<GraphicsObjectType>(MemoryTag);
    if (Object.IsValid() && Object->Init(std::forward<Args>(Arguments)...))
    {
        return Object;
    }
    return {};
}

rndr::ScopePtr<rndr::SwapChain> rndr::GraphicsContext::CreateSwapChain(
    NativeWindowHandle WindowHandle,
    int Width,
    int Height,
    const SwapChainProperties& Props)
{
    return CreateGraphicsObject<SwapChain>("rndr::GraphicsContext: SwapChain", this, WindowHandle,
                                           Width, Height, Props);
}

rndr::ScopePtr<rndr::Shader> rndr::GraphicsContext::CreateShader(const ByteSpan& ShaderContents,
                                                                 const ShaderProperties& Props)
{
    return CreateGraphicsObject<Shader>("rndr::GraphicsContext: Shader", this, ShaderContents,
                                        Props);
}

rndr::ScopePtr<rndr::Image> rndr::GraphicsContext::CreateImage(int Width,
                                                               int Height,
                                                               const ImageProperties& Props,
                                                               ByteSpan InitData)
{
    return CreateGraphicsObject<Image>("rndr::GraphicsContext: Image", this, Width, Height, Props,
                                       InitData);
}

rndr::ScopePtr<rndr::Image> rndr::GraphicsContext::CreateImageArray(int Width,
                                                                    int Height,
                                                                    int ArraySize,
                                                                    const ImageProperties& Props,
                                                                    Span<ByteSpan> InitData)
{
    return CreateGraphicsObject<Image>("rndr::GraphicsContext: ImageArray", this, Width, Height,
                                       ArraySize, Props, InitData);
}

rndr::ScopePtr<rndr::Image> rndr::GraphicsContext::CreateCubeMap(int Width,
                                                                 int Height,
                                                                 const ImageProperties& Props,
                                                                 Span<ByteSpan> InitData)
{
    return CreateGraphicsObject<Image>("rndr::GraphicsContext: CubeMap", this, Width, Height, Props,
                                       InitData);
}

rndr::ScopePtr<rndr::Image> rndr::GraphicsContext::CreateImageForSwapChain(SwapChain* SwapChain,
                                                                           int BufferIndex)
{
    return CreateGraphicsObject<Image>("rndr::GraphicsContext: ImageSwapChain", this, SwapChain,
                                       BufferIndex);
}

rndr::ScopePtr<rndr::Sampler> rndr::GraphicsContext::CreateSampler(const SamplerProperties& Props)
{
    return CreateGraphicsObject<Sampler>("rndr::GraphicsContext: Sampler", this, Props);
}

rndr::ScopePtr<rndr::Buffer> rndr::GraphicsContext::CreateBuffer(const BufferProperties& Props,
                                                                 ByteSpan InitialData)
{
    return CreateGraphicsObject<Buffer>("rndr::GraphicsContext: Buffer", this, Props, InitialData);
}

rndr::ScopePtr<rndr::FrameBuffer>
rndr::GraphicsContext::CreateFrameBuffer(int Width, int Height, const FrameBufferProperties& Props)
{
    return CreateGraphicsObject<FrameBuffer>("rndr::GraphicsContext: FrameBuffer", this, Width,
                                             Height, Props);
}

rndr::ScopePtr<rndr::FrameBuffer>
rndr::GraphicsContext::CreateFrameBufferForSwapChain(int Width, int Height, SwapChain* SwapChain)
{
    return CreateGraphicsObject<FrameBuffer>("rndr::GraphicsContext: FrameBuffer SwapChain", this,
                                             Width, Height, SwapChain);
}

rndr::ScopePtr<rndr::InputLayout> rndr::GraphicsContext::CreateInputLayout(
    Span<InputLayoutProperties> Props,
    Shader* Shader)
{
    return CreateGraphicsObject<InputLayout>("rndr::GraphicsContext: InputLayout", this, Props,
                                             Shader);
}

rndr::ScopePtr<rndr::RasterizerState> rndr::GraphicsContext::CreateRasterizerState(
    const RasterizerProperties& Props)
{
    return CreateGraphicsObject<RasterizerState>("rndr::GraphicsContext: RasterizerState", this,
                                                 Props);
}

rndr::ScopePtr<rndr::DepthStencilState> rndr::GraphicsContext::CreateDepthStencilState(
    const DepthStencilProperties& Props)
{
    return CreateGraphicsObject<DepthStencilState>("rndr::GraphicsContext: DepthStencilState", this,
                                                   Props);
}

rndr::ScopePtr<rndr::BlendState> rndr::GraphicsContext::CreateBlendState(
    const BlendProperties& Props)
{
    return CreateGraphicsObject<BlendState>("rndr::GraphicsContext: BlendState", this, Props);
}

rndr::ScopePtr<rndr::Pipeline> rndr::GraphicsContext::CreatePipeline(
    const PipelineProperties& Props)
{
    return CreateGraphicsObject<Pipeline>("rndr::GraphicsContext: Pipeline", this, Props);
}

rndr::ScopePtr<rndr::CommandList> rndr::GraphicsContext::CreateCommandList()
{
    return CreateGraphicsObject<CommandList>("rndr::GraphicsContext: CommandList", this);
}

void rndr::GraphicsContext::ClearColor(Image* Image, math::Vector4 Color)
{
    if ((Image == nullptr) || (Image->DX11RenderTargetView == nullptr))
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
    if ((Image == nullptr) || (Image->DX11DepthStencilView == nullptr))
    {
        RNDR_LOG_ERROR("GraphicsContext::ClearDepth: Invalid image!");
        return;
    }
    m_DeviceContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_DEPTH, Depth,
                                           0);
}

void rndr::GraphicsContext::ClearStencil(Image* Image, uint8_t Stencil)
{
    if ((Image == nullptr) || (Image->DX11DepthStencilView == nullptr))
    {
        RNDR_LOG_ERROR("GraphicsContext::ClearStencil: Invalid image!");
        return;
    }
    m_DeviceContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_STENCIL, 0,
                                           Stencil);
}

void rndr::GraphicsContext::ClearDepthStencil(Image* Image, real Depth, uint8_t Stencil)
{
    if ((Image == nullptr) || (Image->DX11DepthStencilView == nullptr))
    {
        RNDR_LOG_ERROR("GraphicsContext::ClearDepthStencil: Invalid image!");
        return;
    }
    m_DeviceContext->ClearDepthStencilView(Image->DX11DepthStencilView,
                                           D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, Depth, Stencil);
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
        case ShaderType::Compute:
            m_DeviceContext->CSSetShader(Shader->DX11ComputeShader, nullptr, 0);
            break;
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
    if (Shader != nullptr)
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
        DXGI_FORMAT const Format =
            Buffer->Props.Stride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
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
    if (FrameBuffer == nullptr)
    {
        RNDR_LOG_ERROR("GraphicsContext::BindFrameBuffer: Invalid framebuffer!");
        return;
    }

    ID3D11DepthStencilView* DepthStencilView =
        FrameBuffer->DepthStencilBuffer.IsValid()
            ? FrameBuffer->DepthStencilBuffer->DX11DepthStencilView
            : nullptr;
    std::vector<ID3D11RenderTargetView*> RenderTargetViews;
    const int RenderTargetCount = static_cast<int>(FrameBuffer->ColorBuffers.Size);
    RenderTargetViews.resize(RenderTargetCount);

    for (int I = 0; I < RenderTargetCount; I++)
    {
        RenderTargetViews[I] = FrameBuffer->ColorBuffers[I]->DX11RenderTargetView;
    }

    m_DeviceContext->OMSetRenderTargets(RenderTargetCount, RenderTargetViews.data(),
                                        DepthStencilView);
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
    m_DeviceContext->OMSetDepthStencilState(State->DX11DepthStencilState,
                                            State->Props.StencilRefValue);
}

void rndr::GraphicsContext::BindBlendState(BlendState* State)
{
    m_DeviceContext->OMSetBlendState(State->DX11BlendState, nullptr, 0xFFFFFFFF);
}

void rndr::GraphicsContext::BindPipeline(Pipeline* Pipeline)
{
    BindShader(Pipeline->VertexShader.Get());
    BindShader(Pipeline->PixelShader.Get());
    BindInputLayout(Pipeline->InputLayout.Get());
    BindRasterizerState(Pipeline->Rasterizer.Get());
    BindBlendState(Pipeline->Blend.Get());
    BindDepthStencilState(Pipeline->DepthStencil.Get());
}

void rndr::GraphicsContext::DrawIndexed(PrimitiveTopology Topology, int IndicesCount)
{
    m_DeviceContext->IASetPrimitiveTopology(DX11FromPrimitiveTopology(Topology));
    m_DeviceContext->DrawIndexed(IndicesCount, 0, 0);
}

void rndr::GraphicsContext::DrawIndexedInstanced(PrimitiveTopology Topology,
                                                 uint32_t IndexCount,
                                                 uint32_t InstanceCount,
                                                 uint32_t IndexOffset,
                                                 uint32_t InstanceOffset)
{
    m_DeviceContext->IASetPrimitiveTopology(DX11FromPrimitiveTopology(Topology));
    m_DeviceContext->DrawIndexedInstanced(IndexCount, InstanceCount, IndexOffset, 0,
                                          InstanceOffset);
}

void rndr::GraphicsContext::Dispatch(const uint32_t ThreadGroupCountX,
                                     const uint32_t ThreadGroupCountY,
                                     const uint32_t ThreadGroupCountZ)
{
    m_DeviceContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

bool rndr::GraphicsContext::SubmitCommandList(CommandList* List)
{
    if (List == nullptr)
    {
        return false;
    }
    if (!List->IsFinished())
    {
        RNDR_LOG_ERROR(
            "GraphicsContext::SubmitCommandList: User didn't call Finish on the CommandList "
            "object!");
        return false;
    }
    m_DeviceContext->ExecuteCommandList(List->DX11CommandList, 0);
    if (WindowsHasFailed())
    {
        const std::string ErrorMessage = WindowsGetErrorMessage();
        RNDR_LOG_ERROR("GraphicsContext::SubmitCommandList: %s", ErrorMessage.c_str());
        return false;
    }
    return true;
}

void rndr::GraphicsContext::Present(SwapChain* SwapChain, bool BVSync)
{
    // TODO(Marko): Look into ALLOW_TEARING
    const uint32_t Flags = 0;
    const HRESULT Result = SwapChain->DX11SwapChain->Present(static_cast<UINT>(BVSync), Flags);
    if (WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("GraphicsContext::Present: %s", ErrorMessage.c_str());
    }
}

#endif  // RNDR_DX11
