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

void rndr::Surface::SetPixel(const Point2i& Location, uint32_t Color)
{
    assert(Location.X >= 0 && Location.X < m_Width);
    assert(Location.Y >= 0 && Location.Y < m_Height);
    assert(m_ColorBuffer);

    uint32_t* Pixels = (uint32_t*)m_ColorBuffer;
    Color = rndr::Color(Color).ToGammaCorrectSpace().ToUInt();
    Pixels[Location.X + Location.Y * m_Width] = Color;
}

void rndr::Surface::SetPixel(const Point2i& Location, rndr::Color Color)
{
    assert(Location.X >= 0 && Location.X < m_Width);
    assert(Location.Y >= 0 && Location.Y < m_Height);
    assert(m_ColorBuffer);

    uint32_t* Pixels = (uint32_t*)m_ColorBuffer;
    uint32_t ColorInt = Color.ToGammaCorrectSpace().ToUInt();
    Pixels[Location.X + Location.Y * m_Width] = ColorInt;
}

void rndr::Surface::SetPixel(int X, int Y, uint32_t Color)
{
    SetPixel(Point2i{X, Y}, Color);
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

static bool GetBarycentricCoordinates(const rndr::Point3r& Point,
                                      const rndr::Point3r (&Points)[3],
                                      const real TriangleArea,
                                      real (&BarycentricCoordinates)[3])
{
    rndr::Vector3r Edge01 = Points[1] - Points[0];
    rndr::Vector3r Edge12 = Points[2] - Points[1];
    rndr::Vector3r Edge20 = Points[0] - Points[2];
    rndr::Vector3r Edge02 = -Edge20;

    rndr::Vector3r Vec0 = Point - Points[0];
    rndr::Vector3r Vec1 = Point - Points[1];
    rndr::Vector3r Vec2 = Point - Points[2];

    rndr::Vector3r N = rndr::Cross(Edge01, Edge02);

    rndr::Vector3r BarVec0 = rndr::Cross(Edge01, Vec0);
    rndr::Vector3r BarVec1 = rndr::Cross(Edge12, Vec1);
    rndr::Vector3r BarVec2 = rndr::Cross(Edge20, Vec2);

    BarycentricCoordinates[0] = rndr::Cross(Edge01, Vec0).Length() / TriangleArea / 2;
    BarycentricCoordinates[1] = rndr::Cross(Edge12, Vec1).Length() / TriangleArea / 2;
    BarycentricCoordinates[2] = rndr::Cross(Edge20, Vec2).Length() / TriangleArea / 2;

    if (rndr::Dot(N, BarVec0) < 0)
    {
        return false;
    }

    if (rndr::Dot(N, BarVec1) < 0)
    {
        return false;
    }

    if (rndr::Dot(N, BarVec2) < 0)
    {
        return false;
    }

    return true;
}

static real GetTriangleArea(const rndr::Point3r* Points)
{
    rndr::Vector3r Edge01 = Points[1] - Points[0];
    rndr::Vector3r Edge02 = Points[2] - Points[0];

    return rndr::Cross(Edge01, Edge02).Length() / 2;
}

void rndr::Surface::RenderTriangle(const Point2r (&Points2D)[3], PixelShaderCallback Callback)
{
    Point3r Points[3] = {{Points2D[0].X, Points2D[0].Y, 0},
                         {Points2D[1].X, Points2D[1].Y, 0},
                         {Points2D[2].X, Points2D[2].Y, 0}};

    Point3r Min = rndr::Min(rndr::Min(Points[0], Points[1]), Points[2]);
    Point3r Max = rndr::Max(rndr::Max(Points[0], Points[1]), Points[2]);
    Min = rndr::Floor(Min);
    Max = rndr::Ceil(Max);

    Vector3r PixelCenter(0.5, 0.5, 0);

    Min += PixelCenter;
    Max -= PixelCenter;

    real TriangleArea = GetTriangleArea(Points);

    for (real Y = Min.X; Y <= Max.Y; Y += 1)
    {
        for (real X = Min.X; X <= Max.X; X += 1)
        {
            Point2r Position2D{X, Y};
            Point3r Position{X, Y, 0};
            PixelShaderInfo PixelInfo{{X, Y}};
            bool IsInsideTriangle =
                GetBarycentricCoordinates(Position, Points, TriangleArea, PixelInfo.Barycentric);
            if (IsInsideTriangle)
            {
                rndr::Color Color = Callback(PixelInfo);
                Point2i PixelScreen{(int)(Position2D.X - 0.5), (int)(Position2D.Y - 0.5)};
                SetPixel(PixelScreen, Color);
            }
        }
    }
}
