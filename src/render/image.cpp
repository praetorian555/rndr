#include "rndr/render/image.h"

#include "rndr/core/color.h"
#include "rndr/core/math.h"
#include "rndr/core/threading.h"
#include "rndr/core/utilities.h"

#include "rndr/profiling/cputracer.h"

rndr::Image::Image(const ImageConfig& Config) : m_Config(Config)
{
    UpdateSize(Config.Width, Config.Height);
}

void rndr::Image::UpdateSize(int Width, int Height)
{
    RNDR_CPU_TRACE("Image Update Size");

    m_Config.Width = Width;
    m_Config.Height = Height;

    m_Bounds.pMin = Point2i{0, 0};
    m_Bounds.pMax = Point2i{m_Config.Width - 1, m_Config.Height - 1};

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

rndr::Image::~Image()
{
    delete m_Buffer;
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

    // TODO(mkostic): Add support for different sizes of pixels in memory
    const uint32_t* Pixels = (uint32_t*)m_Buffer;
    const uint32_t Value = Pixels[Location.X + Location.Y * m_Config.Width];
    return rndr::Color(Value, m_Config.GammaSpace, m_Config.PixelLayout, true);
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

void rndr::Image::SetPixelColor(const Point2i& Location, rndr::Color Color)
{
    assert(Location.X >= 0 && Location.X < m_Config.Width);
    assert(Location.Y >= 0 && Location.Y < m_Config.Height);
    assert(m_Buffer);

    uint32_t* Pixels = (uint32_t*)m_Buffer;
    Pixels[Location.X + Location.Y * m_Config.Width] =
        Color.ToUInt32(m_Config.GammaSpace, m_Config.PixelLayout);
}

void rndr::Image::SetPixelColor(int X, int Y, rndr::Color Color)
{
    SetPixelColor(Point2i{X, Y}, Color);
}

void rndr::Image::SetPixelColor(const Point2i& Location, uint32_t Color)
{
    assert(Location.X >= 0 && Location.X < m_Config.Width);
    assert(Location.Y >= 0 && Location.Y < m_Config.Height);
    assert(m_Buffer);

    uint32_t* Pixels = (uint32_t*)m_Buffer;
    Pixels[Location.X + Location.Y * m_Config.Width] = Color;
}

void rndr::Image::SetPixelColor(int X, int Y, uint32_t Color)
{
    SetPixelColor(Point2i{X, Y}, Color);
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
    RNDR_CPU_TRACE("Image Clear Color");

    // TODO(mkostic): Add support for different pixel layout sizes
    const uint32_t C = Color.ToUInt32(m_Config.GammaSpace, m_Config.PixelLayout);

    ParallelFor(m_Config.Height, 64,
                [this, C](int RowIndex)
                {
                    uint32_t* Pixels = (uint32_t*)m_Buffer + m_Config.Width * RowIndex;
                    const uint32_t Size = m_Config.Width;
                    for (int i = 0; i < Size; i++)
                    {
                        *Pixels++ = C;
                    }
                });
}

void rndr::Image::ClearDepthBuffer(real ClearValue)
{
    RNDR_CPU_TRACE("Image Clear Depth");

    ParallelFor(m_Config.Height, 64,
                [this, ClearValue](int RowIndex)
                {
                    real* Pixels = (real*)m_Buffer + m_Config.Width * RowIndex;
                    const uint32_t Size = m_Config.Width;
                    for (int i = 0; i < Size; i++)
                    {
                        *Pixels++ = ClearValue;
                    }
                });
}

real rndr::Image::GetAspectRatio() const
{
    return m_Config.Height != 0 ? m_Config.Width / (real)m_Config.Height : 1;
}

void rndr::Image::RenderImage(const rndr::Image& Source, const Point2i& BottomLeft)
{
    RNDR_CPU_TRACE("Image Copy From");

    if (!rndr::Overlaps(m_Bounds, Source.m_Bounds))
    {
        return;
    }

    // This tells us how many pixels to copy along X and Y
    const rndr::Bounds2i OverlapBounds = rndr::Intersect(m_Bounds, Source.m_Bounds);
    Point2i SourceStart{0, 0};
    if (!rndr::Inside(Source.m_Bounds.pMin, m_Bounds))
    {
        Point2i Min{(int)Source.m_Bounds.pMin.X, (int)Source.m_Bounds.pMin.Y};
        if (Min.X < 0)
        {
            SourceStart.X = std::abs(Min.X);
        }
        if (Min.Y < 0)
        {
            SourceStart.Y = std::abs(Min.Y);
        }
    }

    ParallelFor(OverlapBounds.Extent(), 16,
                [&](int X, int Y)
                {
                    Point2i Position{X, Y};
                    Color SourceColor = Source.GetPixelColor(SourceStart + Position);
                    Color DstColor = GetPixelColor(BottomLeft + Position);
                    Color BlendColor = Color::Blend(SourceColor, DstColor);
                    SetPixelColor(BottomLeft + Position, BlendColor);
                });
}

void rndr::Image::SetPixelFormat(rndr::GammaSpace Space, rndr::PixelLayout Layout)
{
    // TODO(mkostic): Add support for layouts that use more or less then 4 bytes
    assert(rndr::GetPixelSize(Layout) == 4);

    ParallelFor(m_Bounds.pMax, 64,
                [&](int X, int Y)
                {
                    const Color OldColor = GetPixelColor(X, Y);
                    const uint32_t NewColor = OldColor.ToUInt32(Space, Layout);
                    SetPixelColor(X, Y, NewColor);
                });

    m_Config.PixelLayout = Layout;
    m_Config.GammaSpace = Space;
}

rndr::Color rndr::Image::Sample(const Point2r& TexCoord, bool Magnified)
{
    rndr::Point2r PixelCoord{(m_Config.Width - 1) * TexCoord.X, (m_Config.Height - 1) * TexCoord.Y};

    rndr::Point2r Min = rndr::Floor(PixelCoord);
    rndr::Point2r Max = rndr::Ceil(PixelCoord);

    Color Result;
    ImageFiltering Filter = Magnified ? m_Config.MagFilter : m_Config.MinFilter;
    if (Filter == ImageFiltering::NearestNeighbor)
    {
        rndr::Point2r Nearest = rndr::Round(PixelCoord);
        Result = GetPixelColor(Nearest.X, Nearest.Y);
    }
    else if (Filter == ImageFiltering::BilinearInterpolation)
    {
    }

    if (Result.GammaSpace == rndr::GammaSpace::GammaCorrected)
    {
        Result = Result.ToLinearSpace();
    }

    return Result;
}
