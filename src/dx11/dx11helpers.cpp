#include "rndr/render/dx11/dx11helpers.h"

#if defined RNDR_DX11

DXGI_FORMAT rndr::DX11FromPixelFormat(PixelFormat Format)
{
    switch (Format)
    {
        case PixelFormat::R8G8B8A8_TYPELESS:
            return DXGI_FORMAT_R8G8B8A8_TYPELESS;
        case PixelFormat::R8_UNORM:
            return DXGI_FORMAT_R8_UNORM;
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
        case PixelFormat::R32G32B32A32_FLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case PixelFormat::R32G32B32_FLOAT:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case PixelFormat::R32G32_FLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;
        case PixelFormat::R32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        default:
        {
            assert(false);
        }
    }

    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

rndr::PixelFormat rndr::DX11ToPixelFormat(DXGI_FORMAT Format)
{
    switch (Format)
    {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            return PixelFormat::R8G8B8A8_TYPELESS;
        case DXGI_FORMAT_R8_UNORM:
            return PixelFormat::R8_UNORM;
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
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return PixelFormat::R32G32B32A32_FLOAT;
        case DXGI_FORMAT_R32G32B32_FLOAT:
            return PixelFormat::R32G32B32_FLOAT;
        case DXGI_FORMAT_R32G32_FLOAT:
            return PixelFormat::R32G32_FLOAT;
        case DXGI_FORMAT_R32_FLOAT:
            return PixelFormat::R32_FLOAT;
        default:
        {
            assert(false);
        }
    }

    return PixelFormat::R8G8B8A8_UNORM;
}

uint32_t rndr::DX11FromUsageToCPUAccess(rndr::Usage Usage)
{
    switch (Usage)
    {
        case rndr::Usage::Default:
            return 0;
        case rndr::Usage::Dynamic:
            return D3D11_CPU_ACCESS_WRITE;
        case rndr::Usage::Readback:
            return D3D11_CPU_ACCESS_READ;
        default:
            assert(false);
    }

    return 0;
}

D3D11_USAGE rndr::DX11FromUsage(Usage Usage)
{
    switch (Usage)
    {
        case Usage::Default:
            return D3D11_USAGE_DEFAULT;
        case Usage::Dynamic:
            return D3D11_USAGE_DYNAMIC;
        case Usage::Readback:
            return D3D11_USAGE_STAGING;
        default:
            assert(false);
    }

    return D3D11_USAGE_DEFAULT;
}

D3D11_PRIMITIVE_TOPOLOGY rndr::DX11FromPrimitiveTopology(PrimitiveTopology Topology)
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

D3D11_INPUT_CLASSIFICATION rndr::DX11FromDataRepetition(DataRepetition Repetition)
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

D3D11_FILTER rndr::DX11FromImageFiltering(ImageFiltering Filter)
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

D3D11_TEXTURE_ADDRESS_MODE rndr::DX11FromImageAddressing(ImageAddressing AddressMode)
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

uint32_t rndr::DX11FromImageBindFlags(uint32_t ImageBindFlags)
{
    uint32_t Result = 0;
    Result |= ImageBindFlags & ImageBindFlags::RenderTarget ? D3D11_BIND_RENDER_TARGET : 0;
    Result |= ImageBindFlags & ImageBindFlags::DepthStencil ? D3D11_BIND_DEPTH_STENCIL : 0;
    Result |= ImageBindFlags & ImageBindFlags::ShaderResource ? D3D11_BIND_SHADER_RESOURCE : 0;
    Result |= ImageBindFlags & ImageBindFlags::UnorderedAccess ? D3D11_BIND_UNORDERED_ACCESS : 0;
    return Result;
}

uint32_t rndr::DX11FromBufferTypeToBindFlag(BufferType Type)
{
    switch (Type)
    {
        case BufferType::Readback:
            return 0;
        case BufferType::Vertex:
            return D3D11_BIND_VERTEX_BUFFER;
        case BufferType::Index:
            return D3D11_BIND_INDEX_BUFFER;
        case BufferType::Constant:
            return D3D11_BIND_CONSTANT_BUFFER;
        case BufferType::UnorderedAccess:
            return D3D11_BIND_UNORDERED_ACCESS;
        default:
            assert(false);
    }

    return D3D11_BIND_VERTEX_BUFFER;
}

D3D11_FILL_MODE rndr::DX11FromFillMode(FillMode Mode)
{
    switch (Mode)
    {
        case FillMode::Solid:
            return D3D11_FILL_SOLID;
        case FillMode::Wireframe:
            return D3D11_FILL_WIREFRAME;
        default:
            assert(false);
    }

    return D3D11_FILL_SOLID;
}

D3D11_CULL_MODE rndr::DX11FromFace(Face Face)
{
    switch (Face)
    {
        case Face::None:
            return D3D11_CULL_NONE;
        case Face::Back:
            return D3D11_CULL_BACK;
        case Face::Front:
            return D3D11_CULL_FRONT;
        default:
            assert(false);
    }

    return D3D11_CULL_NONE;
}

D3D11_COMPARISON_FUNC rndr::DX11FromComparator(Comparator Comp)
{
    switch (Comp)
    {
        case Comparator::Never:
            return D3D11_COMPARISON_NEVER;
        case Comparator::Always:
            return D3D11_COMPARISON_ALWAYS;
        case Comparator::Less:
            return D3D11_COMPARISON_LESS;
        case Comparator::Greater:
            return D3D11_COMPARISON_GREATER;
        case Comparator::Equal:
            D3D11_COMPARISON_EQUAL;
        case Comparator::NotEqual:
            return D3D11_COMPARISON_NOT_EQUAL;
        case Comparator::LessEqual:
            return D3D11_COMPARISON_LESS_EQUAL;
        case Comparator::GreaterEqual:
            return D3D11_COMPARISON_GREATER_EQUAL;
        default:
            assert(false);
    }

    return D3D11_COMPARISON_ALWAYS;
}

D3D11_DEPTH_WRITE_MASK rndr::DX11FromDepthMask(DepthMask Mask)
{
    switch (Mask)
    {
        case DepthMask::None:
            return D3D11_DEPTH_WRITE_MASK_ZERO;
        case DepthMask::All:
            return D3D11_DEPTH_WRITE_MASK_ALL;
        default:
            assert(false);
    }

    return D3D11_DEPTH_WRITE_MASK_ALL;
}

D3D11_STENCIL_OP rndr::DX11FromStencilOperation(StencilOperation Op)
{
    switch (Op)
    {
        case StencilOperation::Keep:
            return D3D11_STENCIL_OP_KEEP;
        case StencilOperation::Invert:
            return D3D11_STENCIL_OP_INVERT;
        case StencilOperation::Zero:
            return D3D11_STENCIL_OP_ZERO;
        case StencilOperation::Replace:
            return D3D11_STENCIL_OP_REPLACE;
        case StencilOperation::Increment:
            return D3D11_STENCIL_OP_INCR;
        case StencilOperation::IncrementWrap:
            return D3D11_STENCIL_OP_INCR_SAT;
        case StencilOperation::Decrement:
            return D3D11_STENCIL_OP_DECR;
        case StencilOperation::DecrementWrap:
            return D3D11_STENCIL_OP_DECR_SAT;
        default:
            assert(false);
    }

    return D3D11_STENCIL_OP_ZERO;
}

D3D11_BLEND rndr::DX11FromBlendFactor(BlendFactor Factor)
{
    switch (Factor)
    {
        case BlendFactor::Zero:
            return D3D11_BLEND_ZERO;
        case BlendFactor::One:
            return D3D11_BLEND_ONE;
        case BlendFactor::SrcColor:
            return D3D11_BLEND_SRC_COLOR;
        case BlendFactor::DstColor:
            return D3D11_BLEND_DEST_COLOR;
        case BlendFactor::InvSrcColor:
            return D3D11_BLEND_INV_SRC_COLOR;
        case BlendFactor::InvDstColor:
            return D3D11_BLEND_INV_DEST_COLOR;
        case BlendFactor::SrcAlpha:
            return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::DstAlpha:
            return D3D11_BLEND_DEST_ALPHA;
        case BlendFactor::InvSrcAlpha:
            return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::InvDstAlpha:
            return D3D11_BLEND_INV_DEST_ALPHA;
        case BlendFactor::ConstColor:
            return D3D11_BLEND_SRC1_COLOR;
        case BlendFactor::InvConstColor:
            return D3D11_BLEND_INV_SRC1_COLOR;
        case BlendFactor::ConstAlpha:
            return D3D11_BLEND_SRC1_ALPHA;
        case BlendFactor::InvConstAlpha:
            return D3D11_BLEND_INV_SRC1_ALPHA;
        default:
            assert(false);
    }

    return D3D11_BLEND_ONE;
}

D3D11_BLEND_OP rndr::DX11FromBlendOperator(BlendOperator Op)
{
    switch (Op)
    {
        case BlendOperator::Add:
            return D3D11_BLEND_OP_ADD;
        case BlendOperator::Subtract:
            return D3D11_BLEND_OP_SUBTRACT;
        case BlendOperator::ReverseSubtract:
            return D3D11_BLEND_OP_REV_SUBTRACT;
        case BlendOperator::Min:
            return D3D11_BLEND_OP_MIN;
        case BlendOperator::Max:
            return D3D11_BLEND_OP_MAX;
        default:
            assert(false);
    }

    return D3D11_BLEND_OP_ADD;
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
