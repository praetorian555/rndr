#include "rndr/core/bitmap.h"

Rndr::Bitmap::Bitmap(int width, int height, int depth, Rndr::PixelFormat pixel_format, const ByteSpan& data)
    : m_width(width), m_height(height), m_depth(depth), m_pixel_format(pixel_format)
{
    if (width <= 0 || height <= 0 || depth <= 0 || !IsPixelFormatSupported(pixel_format))
    {
        m_data.clear();
        m_width = 0;
        m_height = 0;
        m_depth = 0;
        m_comp_count = 0;
        return;
    }

    m_comp_count = FromPixelFormatToComponentCount(pixel_format);
    const size_t buffer_size = GetSize3D();
    m_data.resize(buffer_size);
    const size_t size_to_copy = std::min(data.size(), buffer_size);
    if (size_to_copy > 0)
    {
        memcpy(m_data.data(), data.data(), size_to_copy);
    }
    if (IsComponentLowPrecision(pixel_format))
    {
        m_get_pixel_func = &Bitmap::GetPixelUnsignedByte;
        m_set_pixel_func = &Bitmap::SetPixelUnsignedByte;
    }
    else if (IsComponentHighPrecision(pixel_format))
    {
        m_get_pixel_func = &Bitmap::GetPixelFloat;
        m_set_pixel_func = &Bitmap::SetPixelFloat;
    }
    else
    {
        RNDR_ASSERT(false);
    }
}

size_t Rndr::Bitmap::GetPixelSize() const
{
    return FromPixelFormatToPixelSize(m_pixel_format);
}

Rndr::Vector4f Rndr::Bitmap::GetPixel(int x, int y, int z) const
{
    RNDR_ASSERT(x >= 0 && x < m_width);
    RNDR_ASSERT(y >= 0 && y < m_height);
    RNDR_ASSERT(z >= 0 && z < m_depth);
    RNDR_ASSERT(m_get_pixel_func != nullptr);
    return (*this.*m_get_pixel_func)(x, y, z);
}

void Rndr::Bitmap::SetPixel(int x, int y, int z, const Vector4f& pixel)
{
    RNDR_ASSERT(x >= 0 && x < m_width);
    RNDR_ASSERT(y >= 0 && y < m_height);
    RNDR_ASSERT(z >= 0 && z < m_depth);
    RNDR_ASSERT(m_set_pixel_func != nullptr);
    (*this.*m_set_pixel_func)(x, y, z, pixel);
}

Rndr::Vector4f Rndr::Bitmap::GetPixelUnsignedByte(int x, int y, int z) const
{
    const int offset = (z * m_width * m_height + y * m_width + x) * m_comp_count;
    const uint8_t* pixel = m_data.data() + offset;
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

void Rndr::Bitmap::SetPixelUnsignedByte(int x, int y, int z, const Vector4f& pixel)
{
    const int offset = (z * m_width * m_height + y * m_width + x) * m_comp_count;
    uint8_t* pixel_ptr = m_data.data() + offset;
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

Rndr::Vector4f Rndr::Bitmap::GetPixelFloat(int x, int y, int z) const
{
    const int offset = (z * m_width * m_height + y * m_width + x) * m_comp_count;
    const float* pixel = reinterpret_cast<const float*>(m_data.data()) + offset;
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

void Rndr::Bitmap::SetPixelFloat(int x, int y, int z, const Vector4f& pixel)
{
    const int offset = (z * m_width * m_height + y * m_width + x) * m_comp_count;
    float* pixel_ptr = reinterpret_cast<float*>(m_data.data()) + offset;
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

bool Rndr::Bitmap::IsPixelFormatSupported(Rndr::PixelFormat pixel_format)
{
    return pixel_format == PixelFormat::R8G8B8A8_UNORM || pixel_format == PixelFormat::R8G8B8A8_UNORM_SRGB
           || pixel_format == PixelFormat::R8G8B8_UNORM || pixel_format == PixelFormat::R8G8B8_UNORM_SRGB
           || pixel_format == PixelFormat::R8G8_UNORM || pixel_format == PixelFormat::R8G8_UNORM_SRGB
           || pixel_format == PixelFormat::R8_UNORM || pixel_format == PixelFormat::R8_UNORM_SRGB || pixel_format == PixelFormat::R32_FLOAT
           || pixel_format == PixelFormat::R32G32_FLOAT || pixel_format == PixelFormat::R32G32B32_FLOAT
           || pixel_format == PixelFormat::R32G32B32A32_FLOAT;
}
