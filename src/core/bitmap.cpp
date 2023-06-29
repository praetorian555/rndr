#include "rndr/core/bitmap.h"

Rndr::Bitmap::Bitmap(int width, int height, int depth, Rndr::PixelFormat pixel_format, const uint8_t* data)
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
    const int buffer_size = GetSize3D();
    m_data.resize(buffer_size);
    if (data != nullptr)
    {
        memcpy(m_data.data(), data, buffer_size);
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
        assert(false);
    }
}

int Rndr::Bitmap::GetPixelSize() const
{
    return FromPixelFormatToPixelSize(m_pixel_format);
}

math::Vector4 Rndr::Bitmap::GetPixel(int x, int y, int z) const
{
    assert(x >= 0 && x < m_width);
    assert(y >= 0 && y < m_height);
    assert(z >= 0 && z < m_depth);
    assert(m_get_pixel_func != nullptr);
    return (*this.*m_get_pixel_func)(x, y, z);
}

void Rndr::Bitmap::SetPixel(int x, int y, int z, const math::Vector4& pixel)
{
    assert(x >= 0 && x < m_width);
    assert(y >= 0 && y < m_height);
    assert(z >= 0 && z < m_depth);
    assert(m_set_pixel_func != nullptr);
    (*this.*m_set_pixel_func)(x, y, z, pixel);
}

math::Vector4 Rndr::Bitmap::GetPixelUnsignedByte(int x, int y, int z) const
{
    const int offset = (z * m_width * m_height + y * m_width + x) * m_comp_count;
    const uint8_t* pixel = m_data.data() + offset;
    math::Vector4 result;
    constexpr float k_inv_255 = 1.0f / 255.0f;
    if (m_comp_count > 0)
    {
        result.X = static_cast<float>(pixel[0]) * k_inv_255;
    }
    if (m_comp_count > 1)
    {
        result.Y = static_cast<float>(pixel[1]) * k_inv_255;
    }
    if (m_comp_count > 2)
    {
        result.Z = static_cast<float>(pixel[2]) * k_inv_255;
    }
    if (m_comp_count > 3)
    {
        result.W = static_cast<float>(pixel[3]) * k_inv_255;
    }
    return result;
}

void Rndr::Bitmap::SetPixelUnsignedByte(int x, int y, int z, const math::Vector4& pixel)
{
    const int offset = (z * m_width * m_height + y * m_width + x) * m_comp_count;
    uint8_t* pixel_ptr = m_data.data() + offset;
    constexpr float k_255 = 255.0f;
    if (m_comp_count > 0)
    {
        pixel_ptr[0] = static_cast<uint8_t>(pixel.X * k_255);
    }
    if (m_comp_count > 1)
    {
        pixel_ptr[1] = static_cast<uint8_t>(pixel.Y * k_255);
    }
    if (m_comp_count > 2)
    {
        pixel_ptr[2] = static_cast<uint8_t>(pixel.Z * k_255);
    }
    if (m_comp_count > 3)
    {
        pixel_ptr[3] = static_cast<uint8_t>(pixel.W * k_255);
    }
}

math::Vector4 Rndr::Bitmap::GetPixelFloat(int x, int y, int z) const
{
    const int offset = (z * m_width * m_height + y * m_width + x) * m_comp_count;
    const float* pixel = reinterpret_cast<const float*>(m_data.data()) + offset;
    math::Vector4 result;
    if (m_comp_count > 0)
    {
        result.X = pixel[0];
    }
    if (m_comp_count > 1)
    {
        result.Y = pixel[1];
    }
    if (m_comp_count > 2)
    {
        result.Z = pixel[2];
    }
    if (m_comp_count > 3)
    {
        result.W = pixel[3];
    }
    return result;
}

void Rndr::Bitmap::SetPixelFloat(int x, int y, int z, const math::Vector4& pixel)
{
    const int offset = (z * m_width * m_height + y * m_width + x) * m_comp_count;
    float* pixel_ptr = reinterpret_cast<float*>(m_data.data()) + offset;
    if (m_comp_count > 0)
    {
        pixel_ptr[0] = pixel.X;
    }
    if (m_comp_count > 1)
    {
        pixel_ptr[1] = pixel.Y;
    }
    if (m_comp_count > 2)
    {
        pixel_ptr[2] = pixel.Z;
    }
    if (m_comp_count > 3)
    {
        pixel_ptr[3] = pixel.W;
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
