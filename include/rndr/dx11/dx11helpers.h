#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <d3d11.h>

#include "rndr/core/graphicstypes.h"

namespace rndr
{

template <typename T>
inline void DX11SafeRelease(T& Ptr);

DXGI_FORMAT FromPixelFormat(PixelFormat Format);
PixelFormat ToPixelFormat(DXGI_FORMAT Format);
uint32_t FromCPUAccess(CPUAccess Access);
D3D11_USAGE FromUsage(Usage Usage);
D3D11_PRIMITIVE_TOPOLOGY FromPrimitiveTopology(PrimitiveTopology Topology);
D3D11_INPUT_CLASSIFICATION FromDataRepetition(DataRepetition Repetition);
D3D11_FILTER FromImageFiltering(ImageFiltering Filter);
D3D11_TEXTURE_ADDRESS_MODE FromImageAddressing(ImageAddressing AddressMode);

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
