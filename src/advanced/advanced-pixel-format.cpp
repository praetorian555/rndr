#include "rndr/advanced/advanced-pixel-format.hpp"

VkFormat Rndr::ToVkFormat(AdvancedPixelFormat format)
{
    return static_cast<VkFormat>(format);
}

Rndr::AdvancedPixelFormat Rndr::FromVkFormat(VkFormat format)
{
    return static_cast<AdvancedPixelFormat>(format);
}

Rndr::u32 Rndr::GetPixelSize(AdvancedPixelFormat format)
{
    switch (format)
    {
        // 8-bit formats (1 byte)
        case AdvancedPixelFormat::R4G4_UNORM_PACK8:
        case AdvancedPixelFormat::R8_UNORM:
        case AdvancedPixelFormat::R8_SNORM:
        case AdvancedPixelFormat::R8_USCALED:
        case AdvancedPixelFormat::R8_SSCALED:
        case AdvancedPixelFormat::R8_UINT:
        case AdvancedPixelFormat::R8_SINT:
        case AdvancedPixelFormat::R8_SRGB:
        case AdvancedPixelFormat::S8_UINT:
            return 1;

        // 16-bit formats (2 bytes)
        case AdvancedPixelFormat::R4G4B4A4_UNORM_PACK16:
        case AdvancedPixelFormat::B4G4R4A4_UNORM_PACK16:
        case AdvancedPixelFormat::R5G6B5_UNORM_PACK16:
        case AdvancedPixelFormat::B5G6R5_UNORM_PACK16:
        case AdvancedPixelFormat::R5G5B5A1_UNORM_PACK16:
        case AdvancedPixelFormat::B5G5R5A1_UNORM_PACK16:
        case AdvancedPixelFormat::A1R5G5B5_UNORM_PACK16:
        case AdvancedPixelFormat::R8G8_UNORM:
        case AdvancedPixelFormat::R8G8_SNORM:
        case AdvancedPixelFormat::R8G8_USCALED:
        case AdvancedPixelFormat::R8G8_SSCALED:
        case AdvancedPixelFormat::R8G8_UINT:
        case AdvancedPixelFormat::R8G8_SINT:
        case AdvancedPixelFormat::R8G8_SRGB:
        case AdvancedPixelFormat::R16_UNORM:
        case AdvancedPixelFormat::R16_SNORM:
        case AdvancedPixelFormat::R16_USCALED:
        case AdvancedPixelFormat::R16_SSCALED:
        case AdvancedPixelFormat::R16_UINT:
        case AdvancedPixelFormat::R16_SINT:
        case AdvancedPixelFormat::R16_SFLOAT:
        case AdvancedPixelFormat::D16_UNORM:
            return 2;

        // 24-bit formats (3 bytes)
        case AdvancedPixelFormat::R8G8B8_UNORM:
        case AdvancedPixelFormat::R8G8B8_SNORM:
        case AdvancedPixelFormat::R8G8B8_USCALED:
        case AdvancedPixelFormat::R8G8B8_SSCALED:
        case AdvancedPixelFormat::R8G8B8_UINT:
        case AdvancedPixelFormat::R8G8B8_SINT:
        case AdvancedPixelFormat::R8G8B8_SRGB:
        case AdvancedPixelFormat::B8G8R8_UNORM:
        case AdvancedPixelFormat::B8G8R8_SNORM:
        case AdvancedPixelFormat::B8G8R8_USCALED:
        case AdvancedPixelFormat::B8G8R8_SSCALED:
        case AdvancedPixelFormat::B8G8R8_UINT:
        case AdvancedPixelFormat::B8G8R8_SINT:
        case AdvancedPixelFormat::B8G8R8_SRGB:
        case AdvancedPixelFormat::D16_UNORM_S8_UINT:
            return 3;

        // 32-bit formats (4 bytes)
        case AdvancedPixelFormat::R8G8B8A8_UNORM:
        case AdvancedPixelFormat::R8G8B8A8_SNORM:
        case AdvancedPixelFormat::R8G8B8A8_USCALED:
        case AdvancedPixelFormat::R8G8B8A8_SSCALED:
        case AdvancedPixelFormat::R8G8B8A8_UINT:
        case AdvancedPixelFormat::R8G8B8A8_SINT:
        case AdvancedPixelFormat::R8G8B8A8_SRGB:
        case AdvancedPixelFormat::B8G8R8A8_UNORM:
        case AdvancedPixelFormat::B8G8R8A8_SNORM:
        case AdvancedPixelFormat::B8G8R8A8_USCALED:
        case AdvancedPixelFormat::B8G8R8A8_SSCALED:
        case AdvancedPixelFormat::B8G8R8A8_UINT:
        case AdvancedPixelFormat::B8G8R8A8_SINT:
        case AdvancedPixelFormat::B8G8R8A8_SRGB:
        case AdvancedPixelFormat::A8B8G8R8_UNORM_PACK32:
        case AdvancedPixelFormat::A8B8G8R8_SNORM_PACK32:
        case AdvancedPixelFormat::A8B8G8R8_USCALED_PACK32:
        case AdvancedPixelFormat::A8B8G8R8_SSCALED_PACK32:
        case AdvancedPixelFormat::A8B8G8R8_UINT_PACK32:
        case AdvancedPixelFormat::A8B8G8R8_SINT_PACK32:
        case AdvancedPixelFormat::A8B8G8R8_SRGB_PACK32:
        case AdvancedPixelFormat::A2R10G10B10_UNORM_PACK32:
        case AdvancedPixelFormat::A2R10G10B10_SNORM_PACK32:
        case AdvancedPixelFormat::A2R10G10B10_USCALED_PACK32:
        case AdvancedPixelFormat::A2R10G10B10_SSCALED_PACK32:
        case AdvancedPixelFormat::A2R10G10B10_UINT_PACK32:
        case AdvancedPixelFormat::A2R10G10B10_SINT_PACK32:
        case AdvancedPixelFormat::A2B10G10R10_UNORM_PACK32:
        case AdvancedPixelFormat::A2B10G10R10_SNORM_PACK32:
        case AdvancedPixelFormat::A2B10G10R10_USCALED_PACK32:
        case AdvancedPixelFormat::A2B10G10R10_SSCALED_PACK32:
        case AdvancedPixelFormat::A2B10G10R10_UINT_PACK32:
        case AdvancedPixelFormat::A2B10G10R10_SINT_PACK32:
        case AdvancedPixelFormat::R16G16_UNORM:
        case AdvancedPixelFormat::R16G16_SNORM:
        case AdvancedPixelFormat::R16G16_USCALED:
        case AdvancedPixelFormat::R16G16_SSCALED:
        case AdvancedPixelFormat::R16G16_UINT:
        case AdvancedPixelFormat::R16G16_SINT:
        case AdvancedPixelFormat::R16G16_SFLOAT:
        case AdvancedPixelFormat::R32_UINT:
        case AdvancedPixelFormat::R32_SINT:
        case AdvancedPixelFormat::R32_SFLOAT:
        case AdvancedPixelFormat::B10G11R11_UFLOAT_PACK32:
        case AdvancedPixelFormat::E5B9G9R9_UFLOAT_PACK32:
        case AdvancedPixelFormat::X8_D24_UNORM_PACK32:
        case AdvancedPixelFormat::D32_SFLOAT:
        case AdvancedPixelFormat::D24_UNORM_S8_UINT:
            return 4;

        // 48-bit formats (6 bytes)
        case AdvancedPixelFormat::R16G16B16_UNORM:
        case AdvancedPixelFormat::R16G16B16_SNORM:
        case AdvancedPixelFormat::R16G16B16_USCALED:
        case AdvancedPixelFormat::R16G16B16_SSCALED:
        case AdvancedPixelFormat::R16G16B16_UINT:
        case AdvancedPixelFormat::R16G16B16_SINT:
        case AdvancedPixelFormat::R16G16B16_SFLOAT:
            return 6;

        // 64-bit formats (8 bytes)
        case AdvancedPixelFormat::R16G16B16A16_UNORM:
        case AdvancedPixelFormat::R16G16B16A16_SNORM:
        case AdvancedPixelFormat::R16G16B16A16_USCALED:
        case AdvancedPixelFormat::R16G16B16A16_SSCALED:
        case AdvancedPixelFormat::R16G16B16A16_UINT:
        case AdvancedPixelFormat::R16G16B16A16_SINT:
        case AdvancedPixelFormat::R16G16B16A16_SFLOAT:
        case AdvancedPixelFormat::R32G32_UINT:
        case AdvancedPixelFormat::R32G32_SINT:
        case AdvancedPixelFormat::R32G32_SFLOAT:
        case AdvancedPixelFormat::R64_UINT:
        case AdvancedPixelFormat::R64_SINT:
        case AdvancedPixelFormat::R64_SFLOAT:
        case AdvancedPixelFormat::D32_SFLOAT_S8_UINT:
            return 8;

        // 96-bit formats (12 bytes)
        case AdvancedPixelFormat::R32G32B32_UINT:
        case AdvancedPixelFormat::R32G32B32_SINT:
        case AdvancedPixelFormat::R32G32B32_SFLOAT:
            return 12;

        // 128-bit formats (16 bytes)
        case AdvancedPixelFormat::R32G32B32A32_UINT:
        case AdvancedPixelFormat::R32G32B32A32_SINT:
        case AdvancedPixelFormat::R32G32B32A32_SFLOAT:
        case AdvancedPixelFormat::R64G64_UINT:
        case AdvancedPixelFormat::R64G64_SINT:
        case AdvancedPixelFormat::R64G64_SFLOAT:
            return 16;

        // 192-bit formats (24 bytes)
        case AdvancedPixelFormat::R64G64B64_UINT:
        case AdvancedPixelFormat::R64G64B64_SINT:
        case AdvancedPixelFormat::R64G64B64_SFLOAT:
            return 24;

        // 256-bit formats (32 bytes)
        case AdvancedPixelFormat::R64G64B64A64_UINT:
        case AdvancedPixelFormat::R64G64B64A64_SINT:
        case AdvancedPixelFormat::R64G64B64A64_SFLOAT:
            return 32;

        // Undefined and compressed formats
        case AdvancedPixelFormat::Undefined:
        default:
            return 0;
    }
}

bool Rndr::IsDepthFormat(AdvancedPixelFormat format)
{
    switch (format)
    {
        case AdvancedPixelFormat::D16_UNORM:
        case AdvancedPixelFormat::X8_D24_UNORM_PACK32:
        case AdvancedPixelFormat::D32_SFLOAT:
        case AdvancedPixelFormat::D16_UNORM_S8_UINT:
        case AdvancedPixelFormat::D24_UNORM_S8_UINT:
        case AdvancedPixelFormat::D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
    }
}

bool Rndr::IsStencilFormat(AdvancedPixelFormat format)
{
    switch (format)
    {
        case AdvancedPixelFormat::S8_UINT:
        case AdvancedPixelFormat::D16_UNORM_S8_UINT:
        case AdvancedPixelFormat::D24_UNORM_S8_UINT:
        case AdvancedPixelFormat::D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
    }
}

bool Rndr::IsCompressedFormat(AdvancedPixelFormat format)
{
    const u32 value = static_cast<u32>(format);
    // BC formats: 131-146
    // ETC2/EAC formats: 147-156
    // ASTC formats: 157-184
    return value >= 131 && value <= 184;
}

bool Rndr::IsSrgbFormat(AdvancedPixelFormat format)
{
    switch (format)
    {
        case AdvancedPixelFormat::R8_SRGB:
        case AdvancedPixelFormat::R8G8_SRGB:
        case AdvancedPixelFormat::R8G8B8_SRGB:
        case AdvancedPixelFormat::B8G8R8_SRGB:
        case AdvancedPixelFormat::R8G8B8A8_SRGB:
        case AdvancedPixelFormat::B8G8R8A8_SRGB:
        case AdvancedPixelFormat::A8B8G8R8_SRGB_PACK32:
        case AdvancedPixelFormat::BC1_RGB_SRGB_BLOCK:
        case AdvancedPixelFormat::BC1_RGBA_SRGB_BLOCK:
        case AdvancedPixelFormat::BC2_SRGB_BLOCK:
        case AdvancedPixelFormat::BC3_SRGB_BLOCK:
        case AdvancedPixelFormat::BC7_SRGB_BLOCK:
        case AdvancedPixelFormat::ETC2_R8G8B8_SRGB_BLOCK:
        case AdvancedPixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK:
        case AdvancedPixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_4x4_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_5x4_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_5x5_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_6x5_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_6x6_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_8x5_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_8x6_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_8x8_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_10x5_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_10x6_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_10x8_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_10x10_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_12x10_SRGB_BLOCK:
        case AdvancedPixelFormat::ASTC_12x12_SRGB_BLOCK:
            return true;
        default:
            return false;
    }
}
