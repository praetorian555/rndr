#include "rndr/dx11/dx11pipeline.h"

#if defined RNDR_DX11

#include "rndr/core/log.h"

#include "rndr/dx11/dx11graphicscontext.h"
#include "rndr/dx11/dx11helpers.h"
#include "rndr/dx11/dx11shader.h"

rndr::InputLayout::InputLayout(GraphicsContext* Context, Span<InputLayoutProperties> P, rndr::Shader* Shader)
{
    Props = Span<InputLayoutProperties>(new InputLayoutProperties[P.Size], P.Size);
    for (int i = 0; i < P.Size; i++)
    {
        Props.Data[i] = P.Data[i];
    }

    assert(Props.Size <= GraphicsConstants::MaxInputLayoutEntries);
    D3D11_INPUT_ELEMENT_DESC InputDescriptors[GraphicsConstants::MaxInputLayoutEntries] = {};
    for (int i = 0; i < Props.Size; i++)
    {
        InputDescriptors[i].SemanticName = Props[i].SemanticName.c_str();
        InputDescriptors[i].SemanticIndex = Props[i].SemanticIndex;
        InputDescriptors[i].Format = DX11FromPixelFormat(Props[i].Format);
        InputDescriptors[i].AlignedByteOffset =
            Props[i].OffsetInVertex == AppendAlignedElement ? D3D11_APPEND_ALIGNED_ELEMENT : Props[i].OffsetInVertex;
        InputDescriptors[i].InputSlot = Props[i].InputSlot;
        InputDescriptors[i].InputSlotClass = DX11FromDataRepetition(Props[i].Repetition);
        InputDescriptors[i].InstanceDataStepRate = Props[i].InstanceStepRate;
    }

    ID3D11Device* Device = Context->GetDevice();
    HRESULT Result = Device->CreateInputLayout(InputDescriptors, Props.Size, Shader->DX11ShaderBuffer->GetBufferPointer(),
                                               Shader->DX11ShaderBuffer->GetBufferSize(), &DX11InputLayout);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to setup the input layout!");
        return;
    }
}

rndr::InputLayout::~InputLayout()
{
    delete[] Props.Data;
    DX11SafeRelease(DX11InputLayout);
}

rndr::RasterizerState::RasterizerState(GraphicsContext* Context, const RasterizerProperties& P) : Props(P)
{
    D3D11_RASTERIZER_DESC RasterizerDesc;
    RasterizerDesc.FillMode = DX11FromFillMode(Props.FillMode);
    RasterizerDesc.CullMode = DX11FromFace(Props.CullFace);
    RasterizerDesc.FrontCounterClockwise = Props.FrontFaceWindingOrder == WindingOrder::CCW;
    RasterizerDesc.DepthBias = Props.DepthBias;
    RasterizerDesc.DepthBiasClamp = Props.DepthBiasClamp;
    RasterizerDesc.DepthClipEnable = Props.bDepthClipEnable;
    RasterizerDesc.SlopeScaledDepthBias = Props.SlopeScaledDepthBias;
    RasterizerDesc.AntialiasedLineEnable = Props.bAntialiasedLineEnable;
    RasterizerDesc.ScissorEnable = Props.bScissorEnable;
    RasterizerDesc.MultisampleEnable = Props.bScissorEnable;
    ID3D11Device* Device = Context->GetDevice();
    HRESULT Result = Device->CreateRasterizerState(&RasterizerDesc, &DX11RasterizerState);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to create rasterizer state!");
        return;
    }
}

rndr::RasterizerState::~RasterizerState()
{
    DX11SafeRelease(DX11RasterizerState);
}

rndr::DepthStencilState::DepthStencilState(GraphicsContext* Context, const DepthStencilProperties& P) : Props(P)
{
    D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
    DepthStencilDesc.DepthEnable = Props.bDepthEnable;
    DepthStencilDesc.DepthFunc = DX11FromComparator(Props.DepthComparator);
    DepthStencilDesc.DepthWriteMask = DX11FromDepthMask(Props.DepthMask);
    DepthStencilDesc.StencilEnable = Props.bStencilEnable;
    DepthStencilDesc.StencilReadMask = Props.StencilReadMask;
    DepthStencilDesc.StencilWriteMask = Props.StencilWriteMask;
    DepthStencilDesc.BackFace.StencilFunc = DX11FromComparator(Props.StencilBackFaceComparator);
    DepthStencilDesc.BackFace.StencilFailOp = DX11FromStencilOperation(Props.StencilBackFaceFailOp);
    DepthStencilDesc.BackFace.StencilDepthFailOp = DX11FromStencilOperation(Props.StencilBackFaceDepthFailOp);
    DepthStencilDesc.BackFace.StencilPassOp = DX11FromStencilOperation(Props.StencilBackFacePassOp);
    DepthStencilDesc.FrontFace.StencilFunc = DX11FromComparator(Props.StencilFrontFaceComparator);
    DepthStencilDesc.FrontFace.StencilFailOp = DX11FromStencilOperation(Props.StencilFrontFaceFailOp);
    DepthStencilDesc.FrontFace.StencilDepthFailOp = DX11FromStencilOperation(Props.StencilFrontFaceDepthFailOp);
    DepthStencilDesc.FrontFace.StencilPassOp = DX11FromStencilOperation(Props.StencilFrontFacePassOp);
    ID3D11Device* Device = Context->GetDevice();
    HRESULT Result = Device->CreateDepthStencilState(&DepthStencilDesc, &DX11DepthStencilState);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to create depth stencil state!");
        return;
    }
}

rndr::DepthStencilState::~DepthStencilState()
{
    DX11SafeRelease(DX11DepthStencilState);
}

rndr::BlendState::BlendState(GraphicsContext* Context, const BlendProperties& P) : Props(P)
{
    D3D11_BLEND_DESC BlendDesc;
    ZeroMemory(&BlendDesc, sizeof(D3D11_BLEND_DESC));
    BlendDesc.AlphaToCoverageEnable = false;
    BlendDesc.IndependentBlendEnable = false;
    BlendDesc.RenderTarget[0].BlendEnable = Props.bBlendEnable;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    BlendDesc.RenderTarget[0].SrcBlend = DX11FromBlendFactor(Props.SrcColorFactor);
    BlendDesc.RenderTarget[0].DestBlend = DX11FromBlendFactor(Props.DstColorFactor);
    BlendDesc.RenderTarget[0].BlendOp = DX11FromBlendOperator(Props.ColorOperator);
    BlendDesc.RenderTarget[0].SrcBlendAlpha = DX11FromBlendFactor(Props.SrcAlphaFactor);
    BlendDesc.RenderTarget[0].DestBlendAlpha = DX11FromBlendFactor(Props.DstAlphaFactor);
    BlendDesc.RenderTarget[0].BlendOpAlpha = DX11FromBlendOperator(Props.AlphaOperator);
    ID3D11Device* Device = Context->GetDevice();
    HRESULT Result = Device->CreateBlendState(&BlendDesc, &DX11BlendState);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to create blend state!");
        return;
    }
}

rndr::BlendState::~BlendState()
{
    DX11SafeRelease(DX11BlendState);
}

#endif  // RNDR_DX11
