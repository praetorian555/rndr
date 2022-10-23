#include "rndr/render/dx11/dx11sampler.h"

#if defined RNDR_DX11

#include <d3d.h>

#include "rndr/core/log.h"

#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"

bool rndr::Sampler::Init(GraphicsContext* Context, const SamplerProperties& Props)
{
    this->Props = Props;

    ID3D11Device* Device = Context->GetDevice();

    D3D11_SAMPLER_DESC Desc;
    Desc.AddressU = DX11FromImageAddressing(Props.AddressingU);
    Desc.AddressV = DX11FromImageAddressing(Props.AddressingV);
    Desc.AddressW = DX11FromImageAddressing(Props.AddressingW);
    Desc.BorderColor[0] = Props.WrapBorderColor.X;
    Desc.BorderColor[1] = Props.WrapBorderColor.Y;
    Desc.BorderColor[2] = Props.WrapBorderColor.Z;
    Desc.BorderColor[3] = Props.WrapBorderColor.W;
    Desc.ComparisonFunc = DX11FromComparator(Props.Comp);
    Desc.Filter = DX11FromImageFiltering(Props.Filter);
    Desc.MaxAnisotropy = Props.MaxAnisotropy;
    Desc.MipLODBias = Props.LODBias;
    Desc.MinLOD = Props.MinLOD;
    Desc.MaxLOD = Props.MaxLOD;

    HRESULT Result = Device->CreateSamplerState(&Desc, &DX11State);
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

rndr::Sampler::~Sampler()
{
    DX11SafeRelease(DX11State);
}

#endif  // RNDR_DX11
