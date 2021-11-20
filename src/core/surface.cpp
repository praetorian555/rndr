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

// Bresenhams line-drawing algorithm
void rndr::Surface::RenderLine(const Vector2i& A, const Vector2i& B, uint32_t Color)
{
    Vector2i Start = A;
    Vector2i End = B;

    if (Start.X > End.X)
    {
        std::swap(Start, End);
    }

    int DY = End.Y - Start.Y;
    int DX = End.X - Start.X;

    const int Inc = DY > 0 ? 1 : -1;
    DY = std::abs(DY);

    int Eps = 0;
    if (DY <= DX)
    {
        for (int X = Start.X, Y = Start.Y; X <= End.X; X++)
        {
            SetPixel(X, Y, Color);
            if ((Eps + DY) << 2 < DX)
            {
                Eps += DY;
            }
            else
            {
                Y += Inc;
                Eps += DY - DX;
            }
        }
    }
    else
    {
        for (int Y = Start.Y, X = Start.X; Y * Inc <= End.Y * Inc; Y += Inc)
        {
            SetPixel(X, Y, Color);
            if ((Eps + DX) << 2 < DY)
            {
                Eps += DX;
            }
            else
            {
                X++;
                Eps += DX - DY;
            }
        }
    }
}