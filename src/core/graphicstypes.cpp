#include "rndr/render/graphicstypes.h"

int rndr::GetPixelSize(PixelFormat Format)
{
    switch (Format)
    {
        case PixelFormat::R8_UNORM:
        {
            return 1;
        }
        case PixelFormat::R8G8B8A8_TYPELESS:
        case PixelFormat::R8G8B8A8_UNORM:
        case PixelFormat::R8G8B8A8_UNORM_SRGB:
        case PixelFormat::B8G8R8A8_UNORM:
        case PixelFormat::B8G8R8A8_UNORM_SRGB:
        case PixelFormat::DEPTH24_STENCIL8:
        {
            return 4;
        }
        default:
        {
            assert(false);
        }
    }

    return 4;
}
