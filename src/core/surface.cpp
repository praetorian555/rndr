#include "rndr/core/surface.h"

#include "rndr/core/color.h"
#include "rndr/core/math.h"

rndr::Surface::Surface(const SurfaceOptions& Options) : m_Options(Options)
{
    UpdateSize(Options.Width, Options.Height);
}

uint32_t rndr::Surface::GetPixelSize() const
{
    return rndr::GetPixelSize(m_Options.PixelLayout);
}

rndr::Color rndr::Surface::GetPixelColor(const Point2i& Location) const
{
    assert(Location.X >= 0 && Location.X < m_Options.Width);
    assert(Location.Y >= 0 && Location.Y < m_Options.Height);
    assert(m_ColorBuffer);

    const uint32_t* Pixels = (uint32_t*)m_ColorBuffer;
    const uint32_t Value = Pixels[Location.X + Location.Y * m_Options.Width];
    return rndr::Color(Value);
}

rndr::Color rndr::Surface::GetPixelColor(int X, int Y) const
{
    return GetPixelColor(Point2i{X, Y});
}

real rndr::Surface::GetPixelDepth(const Point2i& Location) const
{
    assert(Location.X >= 0 && Location.X < m_Options.Width);
    assert(Location.Y >= 0 && Location.Y < m_Options.Height);
    assert(m_DepthBuffer);

    const real* Depths = (real*)m_DepthBuffer;
    return Depths[Location.X + Location.Y * m_Options.Width];
}

real rndr::Surface::GetPixelDepth(int X, int Y) const
{
    return GetPixelDepth(Point2i{X, Y});
}

void rndr::Surface::UpdateSize(int Width, int Height)
{
    m_Options.Width = Width;
    m_Options.Height = Height;

    m_ScreenBounds.pMin = Point2i(0, 0);
    m_ScreenBounds.pMax = Point2i(m_Options.Width, m_Options.Height);

    if (m_ColorBuffer)
    {
        delete[] m_ColorBuffer;
        m_ColorBuffer = nullptr;
    }

    if (m_DepthBuffer)
    {
        delete[] m_DepthBuffer;
        m_DepthBuffer = nullptr;
    }

    if (m_Options.Width == 0 || m_Options.Height == 0)
    {
        return;
    }

    uint32_t ByteCount =
        m_Options.Width * m_Options.Height * rndr::GetPixelSize(m_Options.PixelLayout);
    m_ColorBuffer = new uint8_t[ByteCount];

    if (m_Options.bUseDepthBuffer)
    {
        uint32_t ByteCount = m_Options.Width * m_Options.Height * sizeof(real);
        m_DepthBuffer = new uint8_t[ByteCount];
    }
}

void rndr::Surface::SetPixel(const Point2i& Location, rndr::Color Color)
{
    assert(Location.X >= 0 && Location.X < m_Options.Width);
    assert(Location.Y >= 0 && Location.Y < m_Options.Height);
    assert(m_ColorBuffer);

    uint32_t* Pixels = (uint32_t*)m_ColorBuffer;
    Pixels[Location.X + Location.Y * m_Options.Width] = Color.ToUInt();
}

void rndr::Surface::SetPixel(int X, int Y, rndr::Color Color)
{
    SetPixel(Point2i{X, Y}, Color);
}

void rndr::Surface::SetPixelDepth(const Point2i& Location, real Depth)
{
    assert(Location.X >= 0 && Location.X < m_Options.Width);
    assert(Location.Y >= 0 && Location.Y < m_Options.Height);
    assert(m_DepthBuffer);

    real* Depths = (real*)m_DepthBuffer;
    Depths[Location.X + Location.Y * m_Options.Width] = Depth;
}

void rndr::Surface::SetPixelDepth(int X, int Y, real Depth)
{
    SetPixelDepth(Point2i{X, Y}, Depth);
}

// Bresenhams line-drawing algorithm
void rndr::Surface::RenderLine(const Point2i& A, const Point2i& B, rndr::Color Color)
{
    Point2i Start = A;
    Point2i End = B;

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

void rndr::Surface::ClearColorBuffer(rndr::Color Color)
{
    const uint32_t C = Color.ToUInt();
    uint32_t* Pixels = (uint32_t*)m_ColorBuffer;
    const uint32_t Size = m_Options.Width * m_Options.Height;
    for (int i = 0; i < Size; i++)
    {
        *Pixels++ = C;
    }
}

void rndr::Surface::ClearDepthBuffer(real ClearValue)
{
    real* Pixels = (real*)m_DepthBuffer;
    const uint32_t Size = m_Options.Width * m_Options.Height;
    for (int i = 0; i < Size; i++)
    {
        *Pixels++ = ClearValue;
    }
}

void rndr::Surface::RenderBlock(const Point2i& BottomLeft, const Point2i& Size, rndr::Color Color)
{
    assert(BottomLeft.X >= 0 && BottomLeft.X < m_Options.Width);
    assert(BottomLeft.Y >= 0 && BottomLeft.Y < m_Options.Height);
    assert(Size.X > 0);
    assert(Size.Y > 0);

    Point2i TopRight = BottomLeft + Size;
    assert(TopRight.X >= 0 && TopRight.X < m_Options.Width);
    assert(TopRight.Y >= 0 && TopRight.Y < m_Options.Height);

    for (int Y = BottomLeft.Y; Y < TopRight.Y; Y++)
    {
        for (int X = BottomLeft.X; X < TopRight.X; X++)
        {
            SetPixel(X, Y, Color);
        }
    }
}

real rndr::Surface::GetAspectRatio() const
{
    return m_Options.Height != 0 ? m_Options.Width / (real)m_Options.Height : 1;
}
