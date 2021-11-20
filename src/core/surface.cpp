#include "rndr/surface.h"

#include <Windows.h>

void rndr::Surface::UpdateSize(int Width, int Height)
{
    if (m_Width == Width && m_Height == Height)
    {
        return;
    }

    m_Width = Width;
    m_Height = Height;

    if (m_ColorBuffer)
    {
        delete[] m_ColorBuffer;
    }

    m_ColorBuffer = new uint8_t[m_Width * m_Height * m_PixelSize];
}

void rndr::Surface::SetPixel(const Vector2i& Location, uint32_t Color)
{
    assert(Location.X >= 0 && Location.X < m_Width);
    assert(Location.Y >= 0 && Location.Y < m_Height);
    assert(m_ColorBuffer);

    uint32_t* Pixels = (uint32_t*)m_ColorBuffer;
    Pixels[Location.X + Location.Y * m_Width] = Color;
}

void rndr::Surface::SetPixel(int X, int Y, uint32_t Color)
{
    SetPixel(Vector2i{X, Y}, Color);
}