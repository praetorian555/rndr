#include "rndr/dx11/dx11helpers.h"

#if defined RNDR_DX11

DXGI_FORMAT rndr::FromPixelFormat(PixelFormat Format)
{
    switch (Format)
    {
        case PixelFormat::R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::R8G8B8A8_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case PixelFormat::B8G8R8A8_UNORM:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case PixelFormat::B8G8R8A8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case PixelFormat::DEPTH24_STENCIL8:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:
        {
            assert(false);
        }
    }

    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

rndr::PixelFormat rndr::ToPixelFormat(DXGI_FORMAT Format)
{
    switch (Format)
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return PixelFormat::R8G8B8A8_UNORM;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return PixelFormat::R8G8B8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            return PixelFormat::B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            return PixelFormat::B8G8R8A8_UNORM_SRGB;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return PixelFormat::DEPTH24_STENCIL8;
        default:
        {
            assert(false);
        }
    }

    return PixelFormat::R8G8B8A8_UNORM;
}

uint32_t rndr::FromCPUAccess(CPUAccess Access)
{
    switch (Access)
    {
        case CPUAccess::Read:
            return D3D11_CPU_ACCESS_READ;
        case CPUAccess::Write:
            return D3D11_CPU_ACCESS_WRITE;
        default:
            assert(false);
    }

    return 0;
}

D3D11_USAGE rndr::FromUsage(Usage Usage)
{
    switch (Usage)
    {
        case Usage::GPUReadWrite:
            return D3D11_USAGE_DEFAULT;
        case Usage::GPURead:
            return D3D11_USAGE_IMMUTABLE;
        case Usage::GPUReadCPUWrite:
            return D3D11_USAGE_DYNAMIC;
        case Usage::FromGPUToCPU:
            return D3D11_USAGE_STAGING;
        default:
            assert(false);
    }

    return D3D11_USAGE_DEFAULT;
}

D3D11_PRIMITIVE_TOPOLOGY rndr::FromPrimitiveTopology(PrimitiveTopology Topology)
{
    switch (Topology)
    {
        case PrimitiveTopology::PointList:
            return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveTopology::LineList:
            return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveTopology::LineStrip:
            return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveTopology::TriangleList:
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveTopology::TriangleStrip:
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default:
            assert(false);
    }

    return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

D3D11_INPUT_CLASSIFICATION rndr::FromDataRepetition(DataRepetition Repetition)
{
    switch (Repetition)
    {
        case DataRepetition::PerVertex:
            return D3D11_INPUT_PER_VERTEX_DATA;
        case DataRepetition::PerInstance:
            return D3D11_INPUT_PER_INSTANCE_DATA;
        default:
            assert(false);
    }

    return D3D11_INPUT_PER_VERTEX_DATA;
}

D3D11_FILTER rndr::FromImageFiltering(ImageFiltering Filter)
{
    switch (Filter)
    {
        case ImageFiltering::MinMagMipPoint:
            return D3D11_FILTER_MIN_MAG_MIP_POINT;
        case ImageFiltering::MinMagPoint_MipLinear:
            return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        case ImageFiltering::MinPoint_MagLinear_MipPoint:
            return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        case ImageFiltering::MinPoint_MagMipLinear:
            return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        case ImageFiltering::MinLinear_MagMipPoint:
            return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        case ImageFiltering::MinLinear_MagPoint_MipLinear:
            return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        case ImageFiltering::MinMagLinear_MipPoint:
            return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        case ImageFiltering::MinMagMipLinear:
            return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        case ImageFiltering::Anisotropic:
            return D3D11_FILTER_ANISOTROPIC;
        default:
            assert(false);
    }

    return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
}

D3D11_TEXTURE_ADDRESS_MODE rndr::FromImageAddressing(ImageAddressing AddressMode)
{
    switch (AddressMode)
    {
        case ImageAddressing::Repeat:
            return D3D11_TEXTURE_ADDRESS_WRAP;
        case ImageAddressing::MirrorRepeat:
            return D3D11_TEXTURE_ADDRESS_MIRROR;
        case ImageAddressing::Clamp:
            return D3D11_TEXTURE_ADDRESS_CLAMP;
        case ImageAddressing::MirrorOnce:
            return D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
        default:
            assert(false);
    }

    return D3D11_TEXTURE_ADDRESS_WRAP;
}

bool rndr::IsRenderTarget(PixelFormat Format)
{
    return Format != PixelFormat::DEPTH24_STENCIL8;
}

bool rndr::IsDepthStencil(PixelFormat Format)
{
    return Format == PixelFormat::DEPTH24_STENCIL8;
}

#endif  // RNDR_DX11
