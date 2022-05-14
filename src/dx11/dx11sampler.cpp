#include "rndr/dx11/dx11sampler.h"

#if defined RNDR_DX11

#include <d3d.h>

#include "rndr/dx11/dx11graphicscontext.h"
#include "rndr/dx11/dx11helpers.h"

#include "rndr/core/log.h"

rndr::Sampler::Sampler(GraphicsContext* Context, const SamplerProperties& Props) : m_Props(Props)
{
    ID3D11Device* Device = Context->GetDevice();

    D3D11_SAMPLER_DESC Desc;
    Desc.AddressU = DX11FromImageAddressing(m_Props.AddressingU);
    Desc.AddressV = DX11FromImageAddressing(m_Props.AddressingV);
    Desc.AddressW = DX11FromImageAddressing(m_Props.AddressingW);
    Desc.BorderColor[0] = m_Props.WrapBorderColor.X;
    Desc.BorderColor[1] = m_Props.WrapBorderColor.Y;
    Desc.BorderColor[2] = m_Props.WrapBorderColor.Z;
    Desc.BorderColor[3] = m_Props.WrapBorderColor.W;
    Desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;  // TODO(mkostic): Add support for this
    Desc.Filter = DX11FromImageFiltering(m_Props.Filter);
    Desc.MaxAnisotropy = m_Props.MaxAnisotropy;
    Desc.MipLODBias = m_Props.LODBias;
    Desc.MinLOD = m_Props.MinLOD;
    Desc.MaxLOD = m_Props.MaxLOD;

    HRESULT Result = Device->CreateSamplerState(&Desc, &m_State);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR("Failed to create sampler state!");
        return;
    }
}

rndr::Sampler::~Sampler()
{
    DX11SafeRelease(m_State);
}

ID3D11SamplerState* rndr::Sampler::GetSamplerState()
{
    return m_State;
}

#endif  // RNDR_DX11
