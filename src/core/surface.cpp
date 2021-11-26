#include "rndr/surface.h"

#include <Windows.h>

#include "rndr/color.h"

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
    Color = rndr::Color(Color).ToGammaCorrectSpace().ToUInt();
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

void rndr::Surface::ClearColorBuffer(uint32_t Color)
{
    for (uint32_t Y = 0; Y < m_Height; Y++)
    {
        for (uint32_t X = 0; X < m_Width; X++)
        {
            SetPixel(X, Y, Color);
        }
    }
}

void rndr::Surface::RenderBlock(const Vector2i& BottomLeft, const Vector2i& Size, uint32_t Color)
{
    assert(BottomLeft.X >= 0 && BottomLeft.X < m_Width);
    assert(BottomLeft.Y >= 0 && BottomLeft.Y < m_Height);
    assert(Size.X > 0);
    assert(Size.Y > 0);

    Vector2i TopRight = BottomLeft + Size;
    assert(TopRight.X >= 0 && TopRight.X < m_Width);
    assert(TopRight.Y >= 0 && TopRight.Y < m_Height);

    for (int Y = BottomLeft.Y; Y < TopRight.Y; Y++)
    {
        for (int X = BottomLeft.X; X < TopRight.X; X++)
        {
            SetPixel(X, Y, Color);
        }
    }
}
