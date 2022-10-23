#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <d3d11.h>

#include "rndr/render/graphicstypes.h"

namespace rndr
{

template <typename T>
inline void DX11SafeRelease(T& Ptr);

DXGI_FORMAT DX11FromPixelFormat(PixelFormat Format);
PixelFormat DX11ToPixelFormat(DXGI_FORMAT Format);
uint32_t DX11FromUsageToCPUAccess(rndr::Usage Usage);
D3D11_USAGE DX11FromUsage(Usage Usage);
D3D11_PRIMITIVE_TOPOLOGY DX11FromPrimitiveTopology(PrimitiveTopology Topology);
D3D11_INPUT_CLASSIFICATION DX11FromDataRepetition(DataRepetition Repetition);
D3D11_FILTER DX11FromImageFiltering(ImageFiltering Filter);
D3D11_TEXTURE_ADDRESS_MODE DX11FromImageAddressing(ImageAddressing AddressMode);
uint32_t DX11FromImageBindFlags(uint32_t ImageBindFlags);
uint32_t DX11FromBufferBindFlag(BufferBindFlag Flag);
D3D11_FILL_MODE DX11FromFillMode(FillMode Mode);
D3D11_CULL_MODE DX11FromFace(Face Face);
D3D11_COMPARISON_FUNC DX11FromComparator(Comparator Comp);
D3D11_DEPTH_WRITE_MASK DX11FromDepthMask(DepthMask Mask);
D3D11_STENCIL_OP DX11FromStencilOperation(StencilOperation Op);
D3D11_BLEND DX11FromBlendFactor(BlendFactor Factor);
D3D11_BLEND_OP DX11FromBlendOperator(BlendOperator Op);

bool IsRenderTarget(PixelFormat Format);
bool IsDepthStencil(PixelFormat Format);

}  // namespace rndr

// Implementations

template <typename T>
inline void rndr::DX11SafeRelease(T& Ptr)
{
    if (Ptr)
    {
        Ptr->Release();
        Ptr = nullptr;
    }
}

#endif  // RNDR_DX11
