#include "rndr/render/dx11/dx11pipeline.h"

#if defined RNDR_DX11

#include "rndr/core/log.h"

#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/dx11/dx11shader.h"

bool rndr::InputLayout::Init(GraphicsContext* Context, Span<InputLayoutProperties> Props, rndr::Shader* Shader)
{
    if (!Context)
    {
        RNDR_LOG_ERROR("InputLayout::Init: Invalid graphics context!");
        return false;
    }
    if (!Props)
    {
        RNDR_LOG_ERROR("InputLayout::Init: No entries!");
        return false;
    }
    if (Props.Size > GraphicsConstants::MaxInputLayoutEntries)
    {
        RNDR_LOG_ERROR("InputLayout::Init: Too many entries!");
        return false;
    }
    if (!Shader)
    {
        RNDR_LOG_ERROR("InputLayout::Init: Invalid shader!");
        return false;
    }

    this->Props = Span<InputLayoutProperties>(new InputLayoutProperties[Props.Size], Props.Size);
    for (int i = 0; i < Props.Size; i++)
    {
        this->Props.Data[i] = Props.Data[i];
    }

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
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("InputLayout::Init: %s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

rndr::InputLayout::~InputLayout()
{
    if (Props)
    {
        delete[] Props.Data;
    }
    DX11SafeRelease(DX11InputLayout);
}

bool rndr::RasterizerState::Init(GraphicsContext* Context, const RasterizerProperties& Props)
{
    if (!Context)
    {
        RNDR_LOG_ERROR("RasterizerState::Init: Invalid graphics context!");
        return false;
    }

    this->Props = Props;

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
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("RasterizerState::Init: %s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

rndr::RasterizerState::~RasterizerState()
{
    DX11SafeRelease(DX11RasterizerState);
}

bool rndr::DepthStencilState::Init(GraphicsContext* Context, const DepthStencilProperties& Props)
{
    if (!Context)
    {
        RNDR_LOG_ERROR("DepthStencilState::Init: Invalid graphics context!");
        return false;
    }

    this->Props = Props;

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
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("DepthStencilState::Init: %s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

rndr::DepthStencilState::~DepthStencilState()
{
    DX11SafeRelease(DX11DepthStencilState);
}

bool rndr::BlendState::Init(GraphicsContext* Context, const BlendProperties& Props)
{
    if (!Context)
    {
        RNDR_LOG_ERROR("BlendState::Init: Invalid graphics context!");
        return false;
    }

    this->Props = Props;

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
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("BlendState::Init: %s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

rndr::BlendState::~BlendState()
{
    DX11SafeRelease(DX11BlendState);
}

#endif  // RNDR_DX11
