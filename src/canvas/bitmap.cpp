#include "rndr/canvas/bitmap.hpp"

#include "opal/math-base.h"

Rndr::i32 Rndr::Canvas::Bitmap::GetFormatPixelSize(Format format)
{
    switch (format)
    {
        case Format::R8:
            return 1;
        case Format::RG8:
            return 2;
        case Format::RGB8:
        case Format::SRGB8:
            return 3;
        case Format::RGBA8:
        case Format::SRGBA8:
            return 4;
        case Format::R16F:
            return 2;
        case Format::RG16F:
            return 4;
        case Format::RGBA16F:
            return 8;
        case Format::R32F:
            return 4;
        case Format::RG32F:
            return 8;
        case Format::RGBA32F:
            return 16;
        default:
            return 0;
    }
}

Rndr::i32 Rndr::Canvas::Bitmap::GetFormatComponentCount(Format format)
{
    switch (format)
    {
        case Format::R8:
        case Format::R16F:
        case Format::R32F:
            return 1;
        case Format::RG8:
        case Format::RG16F:
        case Format::RG32F:
            return 2;
        case Format::RGB8:
        case Format::SRGB8:
            return 3;
        case Format::RGBA8:
        case Format::SRGBA8:
        case Format::RGBA16F:
        case Format::RGBA32F:
            return 4;
        default:
            return 0;
    }
}

bool Rndr::Canvas::Bitmap::IsFormatSupported(Format format)
{
    return GetFormatPixelSize(format) > 0;
}

static bool IsUnsignedByteFormat(Rndr::Canvas::Format format)
{
    switch (format)
    {
        case Rndr::Canvas::Format::R8:
        case Rndr::Canvas::Format::RG8:
        case Rndr::Canvas::Format::RGB8:
        case Rndr::Canvas::Format::RGBA8:
        case Rndr::Canvas::Format::SRGB8:
        case Rndr::Canvas::Format::SRGBA8:
            return true;
        default:
            return false;
    }
}

static bool IsFloatFormat(Rndr::Canvas::Format format)
{
    switch (format)
    {
        case Rndr::Canvas::Format::R32F:
        case Rndr::Canvas::Format::RG32F:
        case Rndr::Canvas::Format::RGBA32F:
            return true;
        default:
            return false;
    }
}

Rndr::Canvas::Bitmap::Bitmap(i32 width, i32 height, i32 depth, Format format, const Opal::ArrayView<const u8>& data)
    : m_width(width), m_height(height), m_depth(depth), m_format(format)
{
    if (width <= 0 || height <= 0 || depth <= 0 || !IsFormatSupported(format))
    {
        m_width = 0;
        m_height = 0;
        m_depth = 0;
        throw Opal::Exception("Invalid input argument when creating Canvas::Bitmap");
    }

    m_comp_count = GetFormatComponentCount(format);
    m_pixel_size = GetFormatPixelSize(format);

    const u64 buffer_size = GetTotalSize();
    m_data.Resize(buffer_size);
    if (!data.IsEmpty())
    {
        const u64 copy_size = Opal::Min(data.GetSize(), buffer_size);
        memcpy(m_data.GetData(), data.GetData(), copy_size);
    }

    if (IsUnsignedByteFormat(format))
    {
        m_get_pixel_func = &Bitmap::GetPixelUnsignedByte;
        m_set_pixel_func = &Bitmap::SetPixelUnsignedByte;
    }
    else if (IsFloatFormat(format))
    {
        m_get_pixel_func = &Bitmap::GetPixelFloat;
        m_set_pixel_func = &Bitmap::SetPixelFloat;
    }
    else
    {
        // Half-float formats: no CPU-side pixel access.
        m_get_pixel_func = nullptr;
        m_set_pixel_func = nullptr;
    }
}

Rndr::Vector4f Rndr::Canvas::Bitmap::GetPixel(i32 x, i32 y, i32 z) const
{
    RNDR_ASSERT(IsValid(), "Bitmap is not valid!");
    RNDR_ASSERT(x >= 0 && x < m_width, "X out of bounds!");
    RNDR_ASSERT(y >= 0 && y < m_height, "Y out of bounds!");
    RNDR_ASSERT(z >= 0 && z < m_depth, "Z out of bounds!");
    RNDR_ASSERT(m_get_pixel_func != nullptr, "Get pixel not supported for this format!");
    return (this->*m_get_pixel_func)(x, y, z);
}

void Rndr::Canvas::Bitmap::SetPixel(i32 x, i32 y, i32 z, const Vector4f& pixel)
{
    RNDR_ASSERT(IsValid(), "Bitmap is not valid!");
    RNDR_ASSERT(x >= 0 && x < m_width, "X out of bounds!");
    RNDR_ASSERT(y >= 0 && y < m_height, "Y out of bounds!");
    RNDR_ASSERT(z >= 0 && z < m_depth, "Z out of bounds!");
    RNDR_ASSERT(m_set_pixel_func != nullptr, "Set pixel not supported for this format!");
    (this->*m_set_pixel_func)(x, y, z, pixel);
}

Rndr::Vector4f Rndr::Canvas::Bitmap::GetPixelUnsignedByte(i32 x, i32 y, i32 z) const
{
    const u64 offset = (static_cast<u64>(z) * m_width * m_height + static_cast<u64>(y) * m_width + x) * m_comp_count;
    const u8* pixel = m_data.GetData() + offset;
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

void Rndr::Canvas::Bitmap::SetPixelUnsignedByte(i32 x, i32 y, i32 z, const Vector4f& pixel)
{
    const u64 offset = (static_cast<u64>(z) * m_width * m_height + static_cast<u64>(y) * m_width + x) * m_comp_count;
    u8* pixel_ptr = m_data.GetData() + offset;
    constexpr float k_255 = 255.0f;
    if (m_comp_count > 0)
    {
        pixel_ptr[0] = static_cast<u8>(Opal::Clamp(pixel.x, 0.0f, 1.0f) * k_255);
    }
    if (m_comp_count > 1)
    {
        pixel_ptr[1] = static_cast<u8>(Opal::Clamp(pixel.y, 0.0f, 1.0f) * k_255);
    }
    if (m_comp_count > 2)
    {
        pixel_ptr[2] = static_cast<u8>(Opal::Clamp(pixel.z, 0.0f, 1.0f) * k_255);
    }
    if (m_comp_count > 3)
    {
        pixel_ptr[3] = static_cast<u8>(Opal::Clamp(pixel.w, 0.0f, 1.0f) * k_255);
    }
}

Rndr::Vector4f Rndr::Canvas::Bitmap::GetPixelFloat(i32 x, i32 y, i32 z) const
{
    const u64 offset = (static_cast<u64>(z) * m_width * m_height + static_cast<u64>(y) * m_width + x) * m_comp_count;
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

void Rndr::Canvas::Bitmap::SetPixelFloat(i32 x, i32 y, i32 z, const Vector4f& pixel)
{
    const u64 offset = (static_cast<u64>(z) * m_width * m_height + static_cast<u64>(y) * m_width + x) * m_comp_count;
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
