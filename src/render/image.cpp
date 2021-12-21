#include "rndr/render/image.h"

#include "rndr/core/color.h"
#include "rndr/core/math.h"
#include "rndr/core/utilities.h"

rndr::Image::Image(const ImageConfig& Config) : m_Config(Config)
{
    UpdateSize(Config.Width, Config.Height);
}

uint32_t rndr::Image::GetPixelSize() const
{
    return rndr::GetPixelSize(m_Config.PixelLayout);
}

rndr::Color rndr::Image::GetPixelColor(const Point2i& Location) const
{
    assert(Location.X >= 0 && Location.X < m_Config.Width);
    assert(Location.Y >= 0 && Location.Y < m_Config.Height);
    assert(m_Buffer);

    const uint32_t* Pixels = (uint32_t*)m_Buffer;
    const uint32_t Value = Pixels[Location.X + Location.Y * m_Config.Width];
    return rndr::Color(Value);
}

rndr::Color rndr::Image::GetPixelColor(int X, int Y) const
{
    return GetPixelColor(Point2i{X, Y});
}

real rndr::Image::GetPixelDepth(const Point2i& Location) const
{
    assert(Location.X >= 0 && Location.X < m_Config.Width);
    assert(Location.Y >= 0 && Location.Y < m_Config.Height);
    assert(m_Buffer);

    const real* Depths = (real*)m_Buffer;
    return Depths[Location.X + Location.Y * m_Config.Width];
}

real rndr::Image::GetPixelDepth(int X, int Y) const
{
    return GetPixelDepth(Point2i{X, Y});
}

void rndr::Image::UpdateSize(int Width, int Height)
{
    m_Config.Width = Width;
    m_Config.Height = Height;

    m_Bounds.pMin = Point3r(0, 0, 0);
    m_Bounds.pMax = Point3r(m_Config.Width, m_Config.Height, 1);

    if (m_Buffer)
    {
        delete[] m_Buffer;
        m_Buffer = nullptr;
    }

    if (m_Config.Width == 0 || m_Config.Height == 0)
    {
        return;
    }

    uint32_t PixelSize = rndr::GetPixelSize(m_Config.PixelLayout);
    uint32_t ByteCount = m_Config.Width * m_Config.Height * PixelSize;
    m_Buffer = new uint8_t[ByteCount];
}

void rndr::Image::SetPixel(const Point2i& Location, rndr::Color Color)
{
    assert(Location.X >= 0 && Location.X < m_Config.Width);
    assert(Location.Y >= 0 && Location.Y < m_Config.Height);
    assert(m_Buffer);

    uint32_t* Pixels = (uint32_t*)m_Buffer;
    Pixels[Location.X + Location.Y * m_Config.Width] = Color.ToUInt();
}

void rndr::Image::SetPixel(int X, int Y, rndr::Color Color)
{
    SetPixel(Point2i{X, Y}, Color);
}

void rndr::Image::SetPixelDepth(const Point2i& Location, real Depth)
{
    assert(Location.X >= 0 && Location.X < m_Config.Width);
    assert(Location.Y >= 0 && Location.Y < m_Config.Height);
    assert(m_Buffer);

    real* Depths = (real*)m_Buffer;
    Depths[Location.X + Location.Y * m_Config.Width] = Depth;
}

void rndr::Image::SetPixelDepth(int X, int Y, real Depth)
{
    SetPixelDepth(Point2i{X, Y}, Depth);
}

void rndr::Image::ClearColorBuffer(rndr::Color Color)
{
    const uint32_t C = Color.ToUInt();
    uint32_t* Pixels = (uint32_t*)m_Buffer;
    const uint32_t Size = m_Config.Width * m_Config.Height;
    for (int i = 0; i < Size; i++)
    {
        *Pixels++ = C;
    }
}

void rndr::Image::ClearDepthBuffer(real ClearValue)
{
    real* Pixels = (real*)m_Buffer;
    const uint32_t Size = m_Config.Width * m_Config.Height;
    for (int i = 0; i < Size; i++)
    {
        *Pixels++ = ClearValue;
    }
}

real rndr::Image::GetAspectRatio() const
{
    return m_Config.Height != 0 ? m_Config.Width / (real)m_Config.Height : 1;
}
