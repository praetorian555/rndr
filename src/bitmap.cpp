#include "rndr/bitmap.hpp"

#include "stb_image/stb_image_resize2.h"

Rndr::Bitmap::Bitmap(i32 width, i32 height, i32 depth, PixelFormat pixel_format, i32 mip_count, const Opal::ArrayView<u8>& data)
    : m_width(width), m_height(height), m_depth(depth), m_mip_count(mip_count), m_pixel_format(pixel_format)
{
    if (width <= 0 || height <= 0 || depth <= 0 || mip_count < 1 || !IsPixelFormatSupported(pixel_format))
    {
        throw Opal::Exception("Invalid input argument when creating bitmap");
    }

    m_comp_count = static_cast<i32>(Rndr::GetComponentCount(pixel_format));
    const u64 buffer_size = GetTotalSize();
    m_data.Resize(buffer_size);
    if (!data.IsEmpty())
    {
        const u64 copy_size = data.GetSize() < buffer_size ? data.GetSize() : buffer_size;
        memcpy(m_data.GetData(), data.GetData(), copy_size);
    }

    if (IsLowPrecisionFormat(pixel_format))
    {
        m_get_pixel_func = &Bitmap::GetPixelUnsignedByte;
        m_set_pixel_func = &Bitmap::SetPixelUnsignedByte;
    }
    else if (IsMediumPrecisionFormat(pixel_format))
    {
        m_get_pixel_func = &Bitmap::GetPixelUnsignedShort;
        m_set_pixel_func = &Bitmap::SetPixelUnsignedShort;
    }
    else if (IsHighPrecisionFormat(pixel_format))
    {
        m_get_pixel_func = &Bitmap::GetPixelFloat;
        m_set_pixel_func = &Bitmap::SetPixelFloat;
    }
    else
    {
        RNDR_ASSERT(false, "Unsupported pixel format precision!");
    }
}

void Rndr::Bitmap::GenerateMips()
{
    if (m_mip_count > 1)
    {
        return;
    }

    auto is_linear_color_space = [](PixelFormat pixel_format)
    {
        return !IsSrgbFormat(pixel_format);
    };

    if (is_linear_color_space(m_pixel_format))
    {
    }
    else
    {

    }
}

Rndr::u64 Rndr::Bitmap::GetPixelSize() const
{
    return Rndr::GetPixelSize(m_pixel_format);
}

Rndr::u64 Rndr::Bitmap::GetRowSize(i32 mip_level) const
{
    if (mip_level >= m_mip_count)
    {
        throw Opal::Exception("Requested mip level does not exist");
    }
    const i32 width = Opal::Max(1, m_width >> mip_level);
    return width * GetPixelSize();
}

Rndr::u64 Rndr::Bitmap::GetSize2D(i32 mip_level) const
{
    if (mip_level >= m_mip_count)
    {
        throw Opal::Exception("Requested mip level does not exist");
    }
    const i32 width = Opal::Max(1, m_width >> mip_level);
    const i32 height = Opal::Max(1, m_height >> mip_level);
    return width * height * GetPixelSize();
}

Rndr::u64 Rndr::Bitmap::GetSize3D(i32 mip_level) const
{
    if (mip_level >= m_mip_count)
    {
        throw Opal::Exception("Requested mip level does not exist");
    }
    const i32 width = Opal::Max(1, m_width >> mip_level);
    const i32 height = Opal::Max(1, m_height >> mip_level);
    const i32 depth = Opal::Max(1, m_depth >> mip_level);
    return width * height * depth * GetPixelSize();
}

Rndr::u64 Rndr::Bitmap::GetTotalSize() const
{
    u64 total_size = m_width * m_height * m_depth * GetPixelSize();
    i64 width = m_width;
    i64 height = m_height;
    i64 depth = m_depth;
    while (width > 1 && height > 1 && depth > 1)
    {
        width = width != 1 ? width >> 1 : 1;
        height = height != 1 ? height >> 1 : 1;
        depth = depth != 1 ? depth >> 1 : 1;
        total_size += width * height * depth * GetPixelSize();
    }
    total_size += GetPixelSize();
    return total_size;
}

Rndr::u64 Rndr::Bitmap::GetMipLevelOffset(i32 mip_level) const
{
    if (mip_level >= m_mip_count)
    {
        throw Opal::Exception("Invalid mip level!");
    }
    if (mip_level == 0)
    {
        return 0;
    }
    u64 offset = 0;
    i32 width = m_width;
    i32 height = m_height;
    i32 depth = m_depth;
    i32 current_mip_level = 0;
    while (current_mip_level < mip_level)
    {
        offset += width * height * depth * GetPixelSize();
        width = Opal::Max(1, width >> 1);
        height = Opal::Max(1, height >> 1);
        depth = Opal::Max(1, depth >> 1);
        current_mip_level++;
    }
    return offset;
}

Rndr::Vector4f Rndr::Bitmap::GetPixel(i32 x, i32 y, i32 z, i32 mip_level) const
{
    RNDR_ASSERT(IsPixelFormatSupported(m_pixel_format), "Pixel Format can't be sampled on the CPU side");
    RNDR_ASSERT(x >= 0 && x < m_width, "X out of bounds!");
    RNDR_ASSERT(y >= 0 && y < m_height, "Y out of bounds!");
    RNDR_ASSERT(z >= 0 && z < m_depth, "Z out of bounds!");
    RNDR_ASSERT(mip_level >= 0 && mip_level < m_mip_count, "Mip level is missing!");
    RNDR_ASSERT(m_get_pixel_func != nullptr, "Get pixel function unavailable!");
    return (*this.*m_get_pixel_func)(x, y, z, mip_level);
}

void Rndr::Bitmap::SetPixel(i32 x, i32 y, i32 z, i32 mip_level, const Vector4f& pixel)
{
    RNDR_ASSERT(IsPixelFormatSupported(m_pixel_format), "Pixel Format doesn't allow modification of individual pixels on the CPU side");
    RNDR_ASSERT(x >= 0 && x < m_width, "X out of bounds!");
    RNDR_ASSERT(y >= 0 && y < m_height, "Y out of bounds!");
    RNDR_ASSERT(z >= 0 && z < m_depth, "Z out of bounds!");
    RNDR_ASSERT(mip_level >= 0 && mip_level < m_mip_count, "Mip level is missing!");
    RNDR_ASSERT(m_set_pixel_func != nullptr, "Set pixel function unavailable!");
    (*this.*m_set_pixel_func)(x, y, z, mip_level, pixel);
}

Rndr::Vector4f Rndr::Bitmap::GetPixelUnsignedByte(i32 x, i32 y, i32 z, i32 mip_level) const
{
    const i32 width = Opal::Max(1, m_width >> mip_level);
    const i32 height = Opal::Max(1, m_height >> mip_level);
    const u64 mip_offset = GetMipLevelOffset(mip_level);
    const u64 offset = mip_offset + (((z * width * height) + (y * width) + x) * m_comp_count);
    const uint8_t* pixel = m_data.GetData() + offset;
    Vector4f result = Vector4f::Zero();
    constexpr float k_inv_255 = 1.0f / 255.0f;
    if (m_comp_count > 0)
    {
        result.x = static_cast<float>(pixel[0]) * k_inv_255;
    }
    if (m_comp_count > 1)
    {
        result.y = static_cast<float>(pixel[1]) * k_inv_255;
    }
    if (m_comp_count > 2)
    {
        result.z = static_cast<float>(pixel[2]) * k_inv_255;
    }
    if (m_comp_count > 3)
    {
        result.w = static_cast<float>(pixel[3]) * k_inv_255;
    }
    return result;
}

void Rndr::Bitmap::SetPixelUnsignedByte(i32 x, i32 y, i32 z, i32 mip_level, const Vector4f& pixel)
{
    const i32 width = Opal::Max(1, m_width >> mip_level);
    const i32 height = Opal::Max(1, m_height >> mip_level);
    const u64 mip_offset = GetMipLevelOffset(mip_level);
    const u64 offset = mip_offset + ((z * width * height + y * width + x) * m_comp_count);
    uint8_t* pixel_ptr = m_data.GetData() + offset;
    constexpr float k_255 = 255.0f;
    if (m_comp_count > 0)
    {
        pixel_ptr[0] = static_cast<uint8_t>(pixel.x * k_255);
    }
    if (m_comp_count > 1)
    {
        pixel_ptr[1] = static_cast<uint8_t>(pixel.y * k_255);
    }
    if (m_comp_count > 2)
    {
        pixel_ptr[2] = static_cast<uint8_t>(pixel.z * k_255);
    }
    if (m_comp_count > 3)
    {
        pixel_ptr[3] = static_cast<uint8_t>(pixel.w * k_255);
    }
}

Rndr::Vector4f Rndr::Bitmap::GetPixelUnsignedShort(i32 x, i32 y, i32 z, i32 mip_level) const
{
    const i32 width = Opal::Max(1, m_width >> mip_level);
    const i32 height = Opal::Max(1, m_height >> mip_level);
    const u64 mip_offset = GetMipLevelOffset(mip_level);
    const u64 offset = mip_offset + (((z * width * height) + (y * width) + x) * m_comp_count * sizeof(u16));
    const u16* pixel = reinterpret_cast<const u16*>(m_data.GetData() + offset);
    Vector4f result = Vector4f::Zero();
    constexpr float k_inv_65535 = 1.0f / 65535.0f;
    if (m_comp_count > 0)
    {
        result.x = static_cast<float>(pixel[0]) * k_inv_65535;
    }
    if (m_comp_count > 1)
    {
        result.y = static_cast<float>(pixel[1]) * k_inv_65535;
    }
    if (m_comp_count > 2)
    {
        result.z = static_cast<float>(pixel[2]) * k_inv_65535;
    }
    if (m_comp_count > 3)
    {
        result.w = static_cast<float>(pixel[3]) * k_inv_65535;
    }
    return result;
}

void Rndr::Bitmap::SetPixelUnsignedShort(i32 x, i32 y, i32 z, i32 mip_level, const Vector4f& pixel)
{
    const i32 width = Opal::Max(1, m_width >> mip_level);
    const i32 height = Opal::Max(1, m_height >> mip_level);
    const u64 mip_offset = GetMipLevelOffset(mip_level);
    const u64 offset = mip_offset + ((z * width * height + y * width + x) * m_comp_count * sizeof(u16));
    u16* pixel_ptr = reinterpret_cast<u16*>(m_data.GetData() + offset);
    constexpr float k_65535 = 65535.0f;
    if (m_comp_count > 0)
    {
        pixel_ptr[0] = static_cast<u16>(pixel.x * k_65535);
    }
    if (m_comp_count > 1)
    {
        pixel_ptr[1] = static_cast<u16>(pixel.y * k_65535);
    }
    if (m_comp_count > 2)
    {
        pixel_ptr[2] = static_cast<u16>(pixel.z * k_65535);
    }
    if (m_comp_count > 3)
    {
        pixel_ptr[3] = static_cast<u16>(pixel.w * k_65535);
    }
}

Rndr::Vector4f Rndr::Bitmap::GetPixelFloat(i32 x, i32 y, i32 z, i32 mip_level) const
{
    const i32 width = Opal::Max(1, m_width >> mip_level);
    const i32 height = Opal::Max(1, m_height >> mip_level);
    const u64 mip_offset = GetMipLevelOffset(mip_level);
    const u64 offset = mip_offset + ((z * width * height + y * width + x) * m_comp_count);
    const float* pixel = reinterpret_cast<const float*>(m_data.GetData()) + offset;
    Vector4f result = Vector4f::Zero();
    if (m_comp_count > 0)
    {
        result.x = pixel[0];
    }
    if (m_comp_count > 1)
    {
        result.y = pixel[1];
    }
    if (m_comp_count > 2)
    {
        result.z = pixel[2];
    }
    if (m_comp_count > 3)
    {
        result.w = pixel[3];
    }
    return result;
}

void Rndr::Bitmap::SetPixelFloat(i32 x, i32 y, i32 z, i32 mip_level, const Vector4f& pixel)
{
    const i32 width = Opal::Max(1, m_width >> mip_level);
    const i32 height = Opal::Max(1, m_height >> mip_level);
    const u64 mip_offset = GetMipLevelOffset(mip_level);
    const u64 offset = mip_offset + ((z * width * height + y * width + x) * m_comp_count);
    float* pixel_ptr = reinterpret_cast<float*>(m_data.GetData()) + offset;
    if (m_comp_count > 0)
    {
        pixel_ptr[0] = pixel.x;
    }
    if (m_comp_count > 1)
    {
        pixel_ptr[1] = pixel.y;
    }
    if (m_comp_count > 2)
    {
        pixel_ptr[2] = pixel.z;
    }
    if (m_comp_count > 3)
    {
        pixel_ptr[3] = pixel.w;
    }
}

bool Rndr::Bitmap::IsPixelFormatSupported(PixelFormat pixel_format)
{
    return IsLowPrecisionFormat(pixel_format) || IsMediumPrecisionFormat(pixel_format) || IsHighPrecisionFormat(pixel_format);
}
