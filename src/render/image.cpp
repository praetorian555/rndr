#include "rndr/render/image.h"

#include "rndr/core/color.h"
#include "rndr/core/math.h"
#include "rndr/core/utilities.h"

rndr::Image::Image(const ImageConfig& Config) : m_Config(Config)
{
    UpdateSize(Config.Width, Config.Height);
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

static rndr::Color BlendColor(rndr::PixelFormat Format, rndr::Color Src, rndr::Color Dst)
{
    if (Format == rndr::PixelFormat::RGB || Format == rndr::PixelFormat::sRGB)
    {
        return Dst;
    }

    if (Format == rndr::PixelFormat::RGBA)
    {
        rndr::Color NewValue;
        real InvColorSrc = 1 - Src.A;

        NewValue.R = Src.A * Src.R + Dst.R * Dst.A * InvColorSrc;
        NewValue.G = Src.A * Src.G + Dst.G * Dst.A * InvColorSrc;
        NewValue.B = Src.A * Src.B + Dst.B * Dst.A * InvColorSrc;
        NewValue.A = Src.A + Dst.A * InvColorSrc;

        return NewValue;
    }

    if (Format == rndr::PixelFormat::sRGBA)
    {
        rndr::Color LinSrc = Src.ToLinearSpace(2.4);
        rndr::Color LinDst = Dst.ToLinearSpace(2.4);

        rndr::Color NewValue;
        real InvColorSrc = 1 - Src.A;

        NewValue.R = LinSrc.A * LinSrc.R + LinDst.R * LinDst.A * InvColorSrc;
        NewValue.G = LinSrc.A * LinSrc.G + LinDst.G * LinDst.A * InvColorSrc;
        NewValue.B = LinSrc.A * LinSrc.B + LinDst.B * LinDst.A * InvColorSrc;
        NewValue.A = LinSrc.A + LinDst.A * InvColorSrc;

        return NewValue.ToGammaCorrectSpace(2.4);
    }

    assert(false);
    return rndr::Color::Pink;
}

void rndr::Image::CopyFrom(const rndr::Image& Source, const Point2i& BottomLeft)
{
    if (!rndr::Overlaps(m_Bounds, Source.m_Bounds))
    {
        return;
    }

    // This tells us how many pixels to copy along X and Y
    const rndr::Bounds3r OverlapBounds = rndr::Intersect(m_Bounds, Source.m_Bounds);
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

    for (int X = 0; X < OverlapBounds.Extent().X; X++)
    {
        for (int Y = 0; Y < OverlapBounds.Extent().Y; Y++)
        {
            Point2i Position{X, Y};
            Color SourceColor = Source.GetPixelColor(SourceStart + Position);
            Color DstColor = GetPixelColor(BottomLeft + Position);
            Color BlendColor = ::BlendColor(m_Config.PixelFormat, SourceColor, DstColor);
            SetPixel(BottomLeft + Position, BlendColor);
        }
    }
}

void rndr::Image::SetPixelLayout(rndr::PixelLayout Layout)
{
    if (m_Config.PixelLayout == Layout)
    {
        return;
    }

    const int OldPixelSize = rndr::GetPixelSize(m_Config.PixelLayout);
    const int NewPixelSize = rndr::GetPixelSize(Layout);

    // TODO(mkostic): Add support for this case
    assert(OldPixelSize == NewPixelSize);

    if (OldPixelSize == 4 && NewPixelSize == 4)
    {
        uint32_t* OldPixels = (uint32_t*)m_Buffer;
        uint32_t* NewPixels = (uint32_t*)m_Buffer;
        const int Count = m_Config.Width * m_Config.Height;
        for (int i = 0; i < Count; i++)
        {
            const Color Color(OldPixels[i], m_Config.PixelLayout);
            NewPixels[i] = Color.ToUInt(Layout);
        }
    }
    else
    {
        // TODO(mkostic): Add support for this case
        assert(false);
    }
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

    return Result;
}
