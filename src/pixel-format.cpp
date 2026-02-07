#include "../include/rndr/pixel-format.hpp"

VkFormat Rndr::ToVkFormat(PixelFormat format)
{
    return static_cast<VkFormat>(format);
}

Rndr::PixelFormat Rndr::FromVkFormat(VkFormat format)
{
    return static_cast<PixelFormat>(format);
}

Rndr::u32 Rndr::GetPixelSize(PixelFormat format)
{
    switch (format)
    {
        // 8-bit formats (1 byte)
        case PixelFormat::R4G4_UNORM_PACK8:
        case PixelFormat::R8_UNORM:
        case PixelFormat::R8_SNORM:
        case PixelFormat::R8_USCALED:
        case PixelFormat::R8_SSCALED:
        case PixelFormat::R8_UINT:
        case PixelFormat::R8_SINT:
        case PixelFormat::R8_SRGB:
        case PixelFormat::S8_UINT:
            return 1;

        // 16-bit formats (2 bytes)
        case PixelFormat::R4G4B4A4_UNORM_PACK16:
        case PixelFormat::B4G4R4A4_UNORM_PACK16:
        case PixelFormat::R5G6B5_UNORM_PACK16:
        case PixelFormat::B5G6R5_UNORM_PACK16:
        case PixelFormat::R5G5B5A1_UNORM_PACK16:
        case PixelFormat::B5G5R5A1_UNORM_PACK16:
        case PixelFormat::A1R5G5B5_UNORM_PACK16:
        case PixelFormat::R8G8_UNORM:
        case PixelFormat::R8G8_SNORM:
        case PixelFormat::R8G8_USCALED:
        case PixelFormat::R8G8_SSCALED:
        case PixelFormat::R8G8_UINT:
        case PixelFormat::R8G8_SINT:
        case PixelFormat::R8G8_SRGB:
        case PixelFormat::R16_UNORM:
        case PixelFormat::R16_SNORM:
        case PixelFormat::R16_USCALED:
        case PixelFormat::R16_SSCALED:
        case PixelFormat::R16_UINT:
        case PixelFormat::R16_SINT:
        case PixelFormat::R16_SFLOAT:
        case PixelFormat::D16_UNORM:
            return 2;

        // 24-bit formats (3 bytes)
        case PixelFormat::R8G8B8_UNORM:
        case PixelFormat::R8G8B8_SNORM:
        case PixelFormat::R8G8B8_USCALED:
        case PixelFormat::R8G8B8_SSCALED:
        case PixelFormat::R8G8B8_UINT:
        case PixelFormat::R8G8B8_SINT:
        case PixelFormat::R8G8B8_SRGB:
        case PixelFormat::B8G8R8_UNORM:
        case PixelFormat::B8G8R8_SNORM:
        case PixelFormat::B8G8R8_USCALED:
        case PixelFormat::B8G8R8_SSCALED:
        case PixelFormat::B8G8R8_UINT:
        case PixelFormat::B8G8R8_SINT:
        case PixelFormat::B8G8R8_SRGB:
        case PixelFormat::D16_UNORM_S8_UINT:
            return 3;

        // 32-bit formats (4 bytes)
        case PixelFormat::R8G8B8A8_UNORM:
        case PixelFormat::R8G8B8A8_SNORM:
        case PixelFormat::R8G8B8A8_USCALED:
        case PixelFormat::R8G8B8A8_SSCALED:
        case PixelFormat::R8G8B8A8_UINT:
        case PixelFormat::R8G8B8A8_SINT:
        case PixelFormat::R8G8B8A8_SRGB:
        case PixelFormat::B8G8R8A8_UNORM:
        case PixelFormat::B8G8R8A8_SNORM:
        case PixelFormat::B8G8R8A8_USCALED:
        case PixelFormat::B8G8R8A8_SSCALED:
        case PixelFormat::B8G8R8A8_UINT:
        case PixelFormat::B8G8R8A8_SINT:
        case PixelFormat::B8G8R8A8_SRGB:
        case PixelFormat::A8B8G8R8_UNORM_PACK32:
        case PixelFormat::A8B8G8R8_SNORM_PACK32:
        case PixelFormat::A8B8G8R8_USCALED_PACK32:
        case PixelFormat::A8B8G8R8_SSCALED_PACK32:
        case PixelFormat::A8B8G8R8_UINT_PACK32:
        case PixelFormat::A8B8G8R8_SINT_PACK32:
        case PixelFormat::A8B8G8R8_SRGB_PACK32:
        case PixelFormat::A2R10G10B10_UNORM_PACK32:
        case PixelFormat::A2R10G10B10_SNORM_PACK32:
        case PixelFormat::A2R10G10B10_USCALED_PACK32:
        case PixelFormat::A2R10G10B10_SSCALED_PACK32:
        case PixelFormat::A2R10G10B10_UINT_PACK32:
        case PixelFormat::A2R10G10B10_SINT_PACK32:
        case PixelFormat::A2B10G10R10_UNORM_PACK32:
        case PixelFormat::A2B10G10R10_SNORM_PACK32:
        case PixelFormat::A2B10G10R10_USCALED_PACK32:
        case PixelFormat::A2B10G10R10_SSCALED_PACK32:
        case PixelFormat::A2B10G10R10_UINT_PACK32:
        case PixelFormat::A2B10G10R10_SINT_PACK32:
        case PixelFormat::R16G16_UNORM:
        case PixelFormat::R16G16_SNORM:
        case PixelFormat::R16G16_USCALED:
        case PixelFormat::R16G16_SSCALED:
        case PixelFormat::R16G16_UINT:
        case PixelFormat::R16G16_SINT:
        case PixelFormat::R16G16_SFLOAT:
        case PixelFormat::R32_UINT:
        case PixelFormat::R32_SINT:
        case PixelFormat::R32_SFLOAT:
        case PixelFormat::B10G11R11_UFLOAT_PACK32:
        case PixelFormat::E5B9G9R9_UFLOAT_PACK32:
        case PixelFormat::X8_D24_UNORM_PACK32:
        case PixelFormat::D32_SFLOAT:
        case PixelFormat::D24_UNORM_S8_UINT:
            return 4;

        // 48-bit formats (6 bytes)
        case PixelFormat::R16G16B16_UNORM:
        case PixelFormat::R16G16B16_SNORM:
        case PixelFormat::R16G16B16_USCALED:
        case PixelFormat::R16G16B16_SSCALED:
        case PixelFormat::R16G16B16_UINT:
        case PixelFormat::R16G16B16_SINT:
        case PixelFormat::R16G16B16_SFLOAT:
            return 6;

        // 64-bit formats (8 bytes)
        case PixelFormat::R16G16B16A16_UNORM:
        case PixelFormat::R16G16B16A16_SNORM:
        case PixelFormat::R16G16B16A16_USCALED:
        case PixelFormat::R16G16B16A16_SSCALED:
        case PixelFormat::R16G16B16A16_UINT:
        case PixelFormat::R16G16B16A16_SINT:
        case PixelFormat::R16G16B16A16_SFLOAT:
        case PixelFormat::R32G32_UINT:
        case PixelFormat::R32G32_SINT:
        case PixelFormat::R32G32_SFLOAT:
        case PixelFormat::R64_UINT:
        case PixelFormat::R64_SINT:
        case PixelFormat::R64_SFLOAT:
        case PixelFormat::D32_SFLOAT_S8_UINT:
            return 8;

        // 96-bit formats (12 bytes)
        case PixelFormat::R32G32B32_UINT:
        case PixelFormat::R32G32B32_SINT:
        case PixelFormat::R32G32B32_SFLOAT:
            return 12;

        // 128-bit formats (16 bytes)
        case PixelFormat::R32G32B32A32_UINT:
        case PixelFormat::R32G32B32A32_SINT:
        case PixelFormat::R32G32B32A32_SFLOAT:
        case PixelFormat::R64G64_UINT:
        case PixelFormat::R64G64_SINT:
        case PixelFormat::R64G64_SFLOAT:
            return 16;

        // 192-bit formats (24 bytes)
        case PixelFormat::R64G64B64_UINT:
        case PixelFormat::R64G64B64_SINT:
        case PixelFormat::R64G64B64_SFLOAT:
            return 24;

        // 256-bit formats (32 bytes)
        case PixelFormat::R64G64B64A64_UINT:
        case PixelFormat::R64G64B64A64_SINT:
        case PixelFormat::R64G64B64A64_SFLOAT:
            return 32;

        // Undefined and compressed formats
        case PixelFormat::Undefined:
        default:
            return 0;
    }
}

bool Rndr::IsDepthFormat(PixelFormat format)
{
    switch (format)
    {
        case PixelFormat::D16_UNORM:
        case PixelFormat::X8_D24_UNORM_PACK32:
        case PixelFormat::D32_SFLOAT:
        case PixelFormat::D16_UNORM_S8_UINT:
        case PixelFormat::D24_UNORM_S8_UINT:
        case PixelFormat::D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
    }
}

bool Rndr::IsStencilFormat(PixelFormat format)
{
    switch (format)
    {
        case PixelFormat::S8_UINT:
        case PixelFormat::D16_UNORM_S8_UINT:
        case PixelFormat::D24_UNORM_S8_UINT:
        case PixelFormat::D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
    }
}

bool Rndr::IsCompressedFormat(PixelFormat format)
{
    const u32 value = static_cast<u32>(format);
    // BC formats: 131-146
    // ETC2/EAC formats: 147-156
    // ASTC formats: 157-184
    return value >= 131 && value <= 184;
}

Rndr::u32 Rndr::GetComponentCount(PixelFormat format)
{
    switch (format)
    {
        // 1-component formats
        case PixelFormat::R8_UNORM:
        case PixelFormat::R8_SNORM:
        case PixelFormat::R8_USCALED:
        case PixelFormat::R8_SSCALED:
        case PixelFormat::R8_UINT:
        case PixelFormat::R8_SINT:
        case PixelFormat::R8_SRGB:
        case PixelFormat::R16_UNORM:
        case PixelFormat::R16_SNORM:
        case PixelFormat::R16_USCALED:
        case PixelFormat::R16_SSCALED:
        case PixelFormat::R16_UINT:
        case PixelFormat::R16_SINT:
        case PixelFormat::R16_SFLOAT:
        case PixelFormat::R32_UINT:
        case PixelFormat::R32_SINT:
        case PixelFormat::R32_SFLOAT:
        case PixelFormat::R64_UINT:
        case PixelFormat::R64_SINT:
        case PixelFormat::R64_SFLOAT:
            return 1;

        // 2-component formats
        case PixelFormat::R8G8_UNORM:
        case PixelFormat::R8G8_SNORM:
        case PixelFormat::R8G8_USCALED:
        case PixelFormat::R8G8_SSCALED:
        case PixelFormat::R8G8_UINT:
        case PixelFormat::R8G8_SINT:
        case PixelFormat::R8G8_SRGB:
        case PixelFormat::R16G16_UNORM:
        case PixelFormat::R16G16_SNORM:
        case PixelFormat::R16G16_USCALED:
        case PixelFormat::R16G16_SSCALED:
        case PixelFormat::R16G16_UINT:
        case PixelFormat::R16G16_SINT:
        case PixelFormat::R16G16_SFLOAT:
        case PixelFormat::R32G32_UINT:
        case PixelFormat::R32G32_SINT:
        case PixelFormat::R32G32_SFLOAT:
        case PixelFormat::R64G64_UINT:
        case PixelFormat::R64G64_SINT:
        case PixelFormat::R64G64_SFLOAT:
            return 2;

        // 3-component formats
        case PixelFormat::R8G8B8_UNORM:
        case PixelFormat::R8G8B8_SNORM:
        case PixelFormat::R8G8B8_USCALED:
        case PixelFormat::R8G8B8_SSCALED:
        case PixelFormat::R8G8B8_UINT:
        case PixelFormat::R8G8B8_SINT:
        case PixelFormat::R8G8B8_SRGB:
        case PixelFormat::B8G8R8_UNORM:
        case PixelFormat::B8G8R8_SNORM:
        case PixelFormat::B8G8R8_USCALED:
        case PixelFormat::B8G8R8_SSCALED:
        case PixelFormat::B8G8R8_UINT:
        case PixelFormat::B8G8R8_SINT:
        case PixelFormat::B8G8R8_SRGB:
        case PixelFormat::R16G16B16_UNORM:
        case PixelFormat::R16G16B16_SNORM:
        case PixelFormat::R16G16B16_USCALED:
        case PixelFormat::R16G16B16_SSCALED:
        case PixelFormat::R16G16B16_UINT:
        case PixelFormat::R16G16B16_SINT:
        case PixelFormat::R16G16B16_SFLOAT:
        case PixelFormat::R32G32B32_UINT:
        case PixelFormat::R32G32B32_SINT:
        case PixelFormat::R32G32B32_SFLOAT:
        case PixelFormat::B10G11R11_UFLOAT_PACK32:
        case PixelFormat::R64G64B64_UINT:
        case PixelFormat::R64G64B64_SINT:
        case PixelFormat::R64G64B64_SFLOAT:
            return 3;

        // 4-component formats
        case PixelFormat::R4G4B4A4_UNORM_PACK16:
        case PixelFormat::B4G4R4A4_UNORM_PACK16:
        case PixelFormat::R5G5B5A1_UNORM_PACK16:
        case PixelFormat::B5G5R5A1_UNORM_PACK16:
        case PixelFormat::A1R5G5B5_UNORM_PACK16:
        case PixelFormat::R8G8B8A8_UNORM:
        case PixelFormat::R8G8B8A8_SNORM:
        case PixelFormat::R8G8B8A8_USCALED:
        case PixelFormat::R8G8B8A8_SSCALED:
        case PixelFormat::R8G8B8A8_UINT:
        case PixelFormat::R8G8B8A8_SINT:
        case PixelFormat::R8G8B8A8_SRGB:
        case PixelFormat::B8G8R8A8_UNORM:
        case PixelFormat::B8G8R8A8_SNORM:
        case PixelFormat::B8G8R8A8_USCALED:
        case PixelFormat::B8G8R8A8_SSCALED:
        case PixelFormat::B8G8R8A8_UINT:
        case PixelFormat::B8G8R8A8_SINT:
        case PixelFormat::B8G8R8A8_SRGB:
        case PixelFormat::A8B8G8R8_UNORM_PACK32:
        case PixelFormat::A8B8G8R8_SNORM_PACK32:
        case PixelFormat::A8B8G8R8_USCALED_PACK32:
        case PixelFormat::A8B8G8R8_SSCALED_PACK32:
        case PixelFormat::A8B8G8R8_UINT_PACK32:
        case PixelFormat::A8B8G8R8_SINT_PACK32:
        case PixelFormat::A8B8G8R8_SRGB_PACK32:
        case PixelFormat::A2R10G10B10_UNORM_PACK32:
        case PixelFormat::A2R10G10B10_SNORM_PACK32:
        case PixelFormat::A2R10G10B10_USCALED_PACK32:
        case PixelFormat::A2R10G10B10_SSCALED_PACK32:
        case PixelFormat::A2R10G10B10_UINT_PACK32:
        case PixelFormat::A2R10G10B10_SINT_PACK32:
        case PixelFormat::A2B10G10R10_UNORM_PACK32:
        case PixelFormat::A2B10G10R10_SNORM_PACK32:
        case PixelFormat::A2B10G10R10_USCALED_PACK32:
        case PixelFormat::A2B10G10R10_SSCALED_PACK32:
        case PixelFormat::A2B10G10R10_UINT_PACK32:
        case PixelFormat::A2B10G10R10_SINT_PACK32:
        case PixelFormat::R16G16B16A16_UNORM:
        case PixelFormat::R16G16B16A16_SNORM:
        case PixelFormat::R16G16B16A16_USCALED:
        case PixelFormat::R16G16B16A16_SSCALED:
        case PixelFormat::R16G16B16A16_UINT:
        case PixelFormat::R16G16B16A16_SINT:
        case PixelFormat::R16G16B16A16_SFLOAT:
        case PixelFormat::R32G32B32A32_UINT:
        case PixelFormat::R32G32B32A32_SINT:
        case PixelFormat::R32G32B32A32_SFLOAT:
        case PixelFormat::E5B9G9R9_UFLOAT_PACK32:
        case PixelFormat::R64G64B64A64_UINT:
        case PixelFormat::R64G64B64A64_SINT:
        case PixelFormat::R64G64B64A64_SFLOAT:
            return 4;

        default:
            return 0;
    }
}

bool Rndr::IsLowPrecisionFormat(PixelFormat format)
{
    switch (format)
    {
        case PixelFormat::R8_UNORM:
        case PixelFormat::R8_SNORM:
        case PixelFormat::R8_USCALED:
        case PixelFormat::R8_SSCALED:
        case PixelFormat::R8_UINT:
        case PixelFormat::R8_SINT:
        case PixelFormat::R8_SRGB:
        case PixelFormat::R8G8_UNORM:
        case PixelFormat::R8G8_SNORM:
        case PixelFormat::R8G8_USCALED:
        case PixelFormat::R8G8_SSCALED:
        case PixelFormat::R8G8_UINT:
        case PixelFormat::R8G8_SINT:
        case PixelFormat::R8G8_SRGB:
        case PixelFormat::R8G8B8_UNORM:
        case PixelFormat::R8G8B8_SNORM:
        case PixelFormat::R8G8B8_USCALED:
        case PixelFormat::R8G8B8_SSCALED:
        case PixelFormat::R8G8B8_UINT:
        case PixelFormat::R8G8B8_SINT:
        case PixelFormat::R8G8B8_SRGB:
        case PixelFormat::B8G8R8_UNORM:
        case PixelFormat::B8G8R8_SNORM:
        case PixelFormat::B8G8R8_USCALED:
        case PixelFormat::B8G8R8_SSCALED:
        case PixelFormat::B8G8R8_UINT:
        case PixelFormat::B8G8R8_SINT:
        case PixelFormat::B8G8R8_SRGB:
        case PixelFormat::R8G8B8A8_UNORM:
        case PixelFormat::R8G8B8A8_SNORM:
        case PixelFormat::R8G8B8A8_USCALED:
        case PixelFormat::R8G8B8A8_SSCALED:
        case PixelFormat::R8G8B8A8_UINT:
        case PixelFormat::R8G8B8A8_SINT:
        case PixelFormat::R8G8B8A8_SRGB:
        case PixelFormat::B8G8R8A8_UNORM:
        case PixelFormat::B8G8R8A8_SNORM:
        case PixelFormat::B8G8R8A8_USCALED:
        case PixelFormat::B8G8R8A8_SSCALED:
        case PixelFormat::B8G8R8A8_UINT:
        case PixelFormat::B8G8R8A8_SINT:
        case PixelFormat::B8G8R8A8_SRGB:
        case PixelFormat::A8B8G8R8_UNORM_PACK32:
        case PixelFormat::A8B8G8R8_SNORM_PACK32:
        case PixelFormat::A8B8G8R8_USCALED_PACK32:
        case PixelFormat::A8B8G8R8_SSCALED_PACK32:
        case PixelFormat::A8B8G8R8_UINT_PACK32:
        case PixelFormat::A8B8G8R8_SINT_PACK32:
        case PixelFormat::A8B8G8R8_SRGB_PACK32:
            return true;
        default:
            return false;
    }
}

bool Rndr::IsMediumPrecisionFormat(PixelFormat format)
{
    switch (format)
    {
        case PixelFormat::R16_UNORM:
        case PixelFormat::R16_SNORM:
        case PixelFormat::R16_USCALED:
        case PixelFormat::R16_SSCALED:
        case PixelFormat::R16_UINT:
        case PixelFormat::R16_SINT:
        case PixelFormat::R16_SFLOAT:
        case PixelFormat::R16G16_UNORM:
        case PixelFormat::R16G16_SNORM:
        case PixelFormat::R16G16_USCALED:
        case PixelFormat::R16G16_SSCALED:
        case PixelFormat::R16G16_UINT:
        case PixelFormat::R16G16_SINT:
        case PixelFormat::R16G16_SFLOAT:
        case PixelFormat::R16G16B16_UNORM:
        case PixelFormat::R16G16B16_SNORM:
        case PixelFormat::R16G16B16_USCALED:
        case PixelFormat::R16G16B16_SSCALED:
        case PixelFormat::R16G16B16_UINT:
        case PixelFormat::R16G16B16_SINT:
        case PixelFormat::R16G16B16_SFLOAT:
        case PixelFormat::R16G16B16A16_UNORM:
        case PixelFormat::R16G16B16A16_SNORM:
        case PixelFormat::R16G16B16A16_USCALED:
        case PixelFormat::R16G16B16A16_SSCALED:
        case PixelFormat::R16G16B16A16_UINT:
        case PixelFormat::R16G16B16A16_SINT:
        case PixelFormat::R16G16B16A16_SFLOAT:
            return true;
        default:
            return false;
    }
}

bool Rndr::IsHighPrecisionFormat(PixelFormat format)
{
    switch (format)
    {
        case PixelFormat::R32_UINT:
        case PixelFormat::R32_SINT:
        case PixelFormat::R32_SFLOAT:
        case PixelFormat::R32G32_UINT:
        case PixelFormat::R32G32_SINT:
        case PixelFormat::R32G32_SFLOAT:
        case PixelFormat::R32G32B32_UINT:
        case PixelFormat::R32G32B32_SINT:
        case PixelFormat::R32G32B32_SFLOAT:
        case PixelFormat::R32G32B32A32_UINT:
        case PixelFormat::R32G32B32A32_SINT:
        case PixelFormat::R32G32B32A32_SFLOAT:
            return true;
        default:
            return false;
    }
}

bool Rndr::IsSrgbFormat(PixelFormat format)
{
    switch (format)
    {
        case PixelFormat::R8_SRGB:
        case PixelFormat::R8G8_SRGB:
        case PixelFormat::R8G8B8_SRGB:
        case PixelFormat::B8G8R8_SRGB:
        case PixelFormat::R8G8B8A8_SRGB:
        case PixelFormat::B8G8R8A8_SRGB:
        case PixelFormat::A8B8G8R8_SRGB_PACK32:
        case PixelFormat::BC1_RGB_SRGB_BLOCK:
        case PixelFormat::BC1_RGBA_SRGB_BLOCK:
        case PixelFormat::BC2_SRGB_BLOCK:
        case PixelFormat::BC3_SRGB_BLOCK:
        case PixelFormat::BC7_SRGB_BLOCK:
        case PixelFormat::ETC2_R8G8B8_SRGB_BLOCK:
        case PixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK:
        case PixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK:
        case PixelFormat::ASTC_4x4_SRGB_BLOCK:
        case PixelFormat::ASTC_5x4_SRGB_BLOCK:
        case PixelFormat::ASTC_5x5_SRGB_BLOCK:
        case PixelFormat::ASTC_6x5_SRGB_BLOCK:
        case PixelFormat::ASTC_6x6_SRGB_BLOCK:
        case PixelFormat::ASTC_8x5_SRGB_BLOCK:
        case PixelFormat::ASTC_8x6_SRGB_BLOCK:
        case PixelFormat::ASTC_8x8_SRGB_BLOCK:
        case PixelFormat::ASTC_10x5_SRGB_BLOCK:
        case PixelFormat::ASTC_10x6_SRGB_BLOCK:
        case PixelFormat::ASTC_10x8_SRGB_BLOCK:
        case PixelFormat::ASTC_10x10_SRGB_BLOCK:
        case PixelFormat::ASTC_12x10_SRGB_BLOCK:
        case PixelFormat::ASTC_12x12_SRGB_BLOCK:
            return true;
        default:
            return false;
    }
}
