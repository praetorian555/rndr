#include "rndr/render/dx11/dx11commandlist.h"

#if defined RNDR_DX11

#include <d3d.h>

#include "rndr/core/log.h"

#include "rndr/render/dx11/dx11buffer.h"
#include "rndr/render/dx11/dx11framebuffer.h"
#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/dx11/dx11image.h"
#include "rndr/render/dx11/dx11pipeline.h"
#include "rndr/render/dx11/dx11sampler.h"
#include "rndr/render/dx11/dx11shader.h"
#include "rndr/render/dx11/dx11swapchain.h"

bool rndr::CommandList::Init(GraphicsContext* Context)
{
    ID3D11Device* DX11Device = Context->GetDevice();
    assert(DX11Device);

    const HRESULT Result = DX11Device->CreateDeferredContext(0, &DX11DeferredContext);
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("CommandList::Init: %s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

rndr::CommandList::~CommandList()
{
    DX11SafeRelease(DX11DeferredContext);
    DX11SafeRelease(DX11CommandList);
}

bool rndr::CommandList::Finish(GraphicsContext* Context)
{
    const HRESULT Result = DX11DeferredContext->FinishCommandList(false, &DX11CommandList);
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("CommandList::Finish: %s", ErrorMessage.c_str());
        return false;
    }
    return true;
}

bool rndr::CommandList::IsFinished() const
{
    return DX11CommandList != nullptr;
}

void rndr::CommandList::ClearColor(Image* Image, math::Vector4 Color)
{
    if (!Image || !Image->DX11RenderTargetView)
    {
        RNDR_LOG_ERROR("CommandList::ClearColor: Invalid image!");
        return;
    }
    DX11DeferredContext->ClearRenderTargetView(Image->DX11RenderTargetView, Color.Data);
}

void rndr::CommandList::ClearDepth(Image* Image, real Depth)
{
    if (!Image || !Image->DX11DepthStencilView)
    {
        RNDR_LOG_ERROR("CommandList::ClearDepth: Invalid image!");
        return;
    }
    DX11DeferredContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_DEPTH, Depth, 0);
}

void rndr::CommandList::ClearStencil(Image* Image, uint8_t Stencil)
{
    if (!Image || !Image->DX11DepthStencilView)
    {
        RNDR_LOG_ERROR("CommandList::ClearStencil: Invalid image!");
        return;
    }
    DX11DeferredContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_STENCIL, 0, Stencil);
}

void rndr::CommandList::ClearDepthStencil(Image* Image, real Depth, uint8_t Stencil)
{
    if (!Image || !Image->DX11DepthStencilView)
    {
        RNDR_LOG_ERROR("CommandList::ClearDepthStencil: Invalid image!");
        return;
    }
    DX11DeferredContext->ClearDepthStencilView(Image->DX11DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, Depth, Stencil);
}

void rndr::CommandList::BindShader(Shader* Shader)
{
    switch (Shader->Props.Type)
    {
        case ShaderType::Vertex:
        {
            DX11DeferredContext->VSSetShader(Shader->DX11VertexShader, nullptr, 0);
            break;
        }
        case ShaderType::Fragment:
        {
            DX11DeferredContext->PSSetShader(Shader->DX11FragmentShader, nullptr, 0);
            break;
        }
        case ShaderType::Compute:
            DX11DeferredContext->CSSetShader(Shader->DX11ComputeShader, nullptr, 0);
            break;
        default:
        {
            assert(false);
        }
    }
}

void rndr::CommandList::BindImageAsShaderResource(Image* Image, int Slot, Shader* Shader)
{
    assert(Shader);
    assert(Image);

    switch (Shader->Props.Type)
    {
        case ShaderType::Vertex:
        {
            DX11DeferredContext->VSSetShaderResources(Slot, 1, &Image->DX11ShaderResourceView);
            break;
        }
        case ShaderType::Fragment:
        {
            DX11DeferredContext->PSSetShaderResources(Slot, 1, &Image->DX11ShaderResourceView);
            break;
        }
        default:
        {
            assert(false);
        }
    }
}

void rndr::CommandList::BindSampler(Sampler* Sampler, int Slot, Shader* Shader)
{
    assert(Shader);
    assert(Sampler);

    switch (Shader->Props.Type)
    {
        case ShaderType::Vertex:
        {
            DX11DeferredContext->VSSetSamplers(Slot, 1, &Sampler->DX11State);
            break;
        }
        case ShaderType::Fragment:
        {
            DX11DeferredContext->PSSetSamplers(Slot, 1, &Sampler->DX11State);
            break;
        }
        default:
        {
            assert(false);
        }
    }
}

void rndr::CommandList::BindBuffer(Buffer* Buffer, int Slot, Shader* Shader)
{
    if (Shader)
    {
        switch (Shader->Props.Type)
        {
            case ShaderType::Vertex:
            {
                DX11DeferredContext->VSSetConstantBuffers(Slot, 1, &Buffer->DX11Buffer);
                break;
            }
            case ShaderType::Fragment:
            {
                DX11DeferredContext->PSSetConstantBuffers(Slot, 1, &Buffer->DX11Buffer);
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
        DX11DeferredContext->IASetIndexBuffer(Buffer->DX11Buffer, Format, 0);
    }
    else
    {
        const uint32_t Stride = Buffer->Props.Stride;
        const uint32_t Offset = 0;
        DX11DeferredContext->IASetVertexBuffers(Slot, 1, &Buffer->DX11Buffer, &Stride, &Offset);
    }
}

void rndr::CommandList::BindFrameBuffer(FrameBuffer* FrameBuffer)
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

    DX11DeferredContext->OMSetRenderTargets(RenderTargetCount, RenderTargetViews.data(), DepthStencilView);
    DX11DeferredContext->RSSetViewports(1, &FrameBuffer->DX11Viewport);
}

void rndr::CommandList::BindInputLayout(InputLayout* InputLayout)
{
    DX11DeferredContext->IASetInputLayout(InputLayout->DX11InputLayout);
}

void rndr::CommandList::BindRasterizerState(RasterizerState* State)
{
    DX11DeferredContext->RSSetState(State->DX11RasterizerState);
}

void rndr::CommandList::BindDepthStencilState(DepthStencilState* State)
{
    DX11DeferredContext->OMSetDepthStencilState(State->DX11DepthStencilState, State->Props.StencilRefValue);
}

void rndr::CommandList::BindBlendState(BlendState* State)
{
    DX11DeferredContext->OMSetBlendState(State->DX11BlendState, nullptr, 0xFFFFFFFF);
}

void rndr::CommandList::DrawIndexed(PrimitiveTopology Topology, int IndicesCount)
{
    DX11DeferredContext->IASetPrimitiveTopology(DX11FromPrimitiveTopology(Topology));
    DX11DeferredContext->DrawIndexed(IndicesCount, 0, 0);
}

void rndr::CommandList::DrawIndexedInstanced(PrimitiveTopology Topology,
                                             int IndexCount,
                                             int InstanceCount,
                                             int IndexOffset,
                                             int InstanceOffset)
{
    DX11DeferredContext->IASetPrimitiveTopology(DX11FromPrimitiveTopology(Topology));
    DX11DeferredContext->DrawIndexedInstanced(IndexCount, InstanceCount, IndexOffset, 0, InstanceOffset);
}

void rndr::CommandList::Dispatch(const uint32_t ThreadGroupCountX, const uint32_t ThreadGroupCountY, const uint32_t ThreadGroupCountZ)
{
    DX11DeferredContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

#endif  // RNDR_DX11
