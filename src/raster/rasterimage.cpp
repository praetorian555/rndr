#include "rndr/raster/rasterimage.h"

#include "stb_image/stb_image.h"

#include "rndr/core/colors.h"
#include "rndr/core/coordinates.h"
#include "rndr/core/fileutils.h"
#include "rndr/core/log.h"
#include "rndr/core/math.h"
#include "rndr/core/threading.h"

#include "rndr/profiling/cputracer.h"

#if defined RNDR_RASTER

rndr::Image::Image(const ImageConfig& Config) : m_Config(Config)
{
    m_Bounds.pMin = Point2i{0, 0};
    m_Bounds.pMax = Point2i{m_Config.Width, m_Config.Height};

    if (m_Config.Width == 0 || m_Config.Height == 0)
    {
        return;
    }

    uint32_t PixelSize = rndr::GetPixelSize(m_Config.PixelLayout);
    uint32_t ByteCount = m_Config.Width * m_Config.Height * PixelSize;
    m_Buffer.resize(ByteCount);

    ParallelFor(ByteCount / PixelSize, 64,
                [&](int i)
                {
                    if (m_Config.PixelLayout != rndr::PixelLayout::DEPTH_F32)
                    {
                        uint32_t* Pixel = (uint32_t*)(m_Buffer.data() + i * PixelSize);
                        Vector4r BackgroundColor = ToGammaCorrectSpace(Colors::Pink);
                        *Pixel = ColorToUInt32(BackgroundColor, m_Config.PixelLayout);
                    }
                    else
                    {
                        real* Pixel = (real*)(m_Buffer.data() + i * PixelSize);
                        *Pixel = -rndr::Infinity;
                    }
                });

    if (m_Config.MinFilter == ImageFiltering::TrilinearInterpolation)
    {
        GenerateMipMaps();
    }
}

rndr::Image::Image(const std::string& FilePath, const ImageConfig& Config) : m_Config(Config)
{
    const ImageFileFormat FileFormat = rndr::GetImageFileFormat(FilePath);
    assert(FileFormat != ImageFileFormat::NotSupported);

    // stb_image library loads data starting from the top left-most pixel while our engine expects
    // data from lower left corner first
    const bool bShouldFlip = true;
    stbi_set_flip_vertically_on_load(bShouldFlip);

    // Note that this one will always return data in a form:
    // 1 channel: Gray
    // 2 channel: Gray Alpha
    // 3 channel: Red Green Blue
    // 4 channel: Red Green Blue Alpha
    //
    // Note that we always request 4 channels.
    int Width, Height, ChannelNumber;
    const int DesiredChannelNumber = 4;
    uint8_t* Data = nullptr;

    Data = stbi_load(FilePath.c_str(), &Width, &Height, &ChannelNumber, DesiredChannelNumber);
    if (!Data)
    {
        RNDR_LOG_ERROR("Image: stbi_load_from_file failed with error: %s", stbi_failure_reason());
        assert(Data);
    }

    m_Config.Width = Width;
    m_Config.Height = Height;

    m_Bounds.pMin = Point2i{0, 0};
    m_Bounds.pMax = Point2i{m_Config.Width, m_Config.Height};

    // TODO(mkostic): How to know if image uses 16 or 8 bits per channel??

    // TODO(mkostic): Add support for grayscale images.
    assert(ChannelNumber == 3 || ChannelNumber == 4);

    const int PixelSize = GetPixelSize();
    const int PixelCount = Width * Height;
    const int BufferSize = Width * Height * PixelSize;

    m_Buffer.resize(BufferSize);

    for (int ByteIndex = 0; ByteIndex < BufferSize; ByteIndex += PixelSize)
    {
        for (int i = 0; i < PixelSize / 2; i++)
        {
            std::swap(Data[ByteIndex + i], Data[ByteIndex + PixelSize - 1 - i]);
        }
    }

    uint32_t* DataU32 = (uint32_t*)Data;
    for (int Y = 0; Y < Height; Y++)
    {
        for (int X = 0; X < Width; X++)
        {
            Vector4r Color = ColorToVector(DataU32[X + Y * Width], PixelLayout::R8G8B8A8);
            Color = ToLinearSpace(Color);
            SetPixelColor(X, Y, Color);
        }
    }

    free(Data);

    if (m_Config.MinFilter == ImageFiltering::TrilinearInterpolation)
    {
        GenerateMipMaps();
    }

    RNDR_LOG_INFO("Successfully loaded image from file: %s, Width=%d, Height=%d, UsesMipMaps=%s", FilePath.c_str(), m_Config.Width,
                  m_Config.Height, m_MipMaps.size() > 0 ? "YES" : "NO");
}

rndr::Image::~Image()
{
    for (int i = 0; i < m_MipMaps.size(); i++)
    {
        if (i != 0)
        {
            delete m_MipMaps[i];
        }
    }
}

uint32_t rndr::Image::GetPixelSize() const
{
    return rndr::GetPixelSize(m_Config.PixelLayout);
}

rndr::Vector4r rndr::Image::GetPixelColor(const Point2i& Location) const
{
    assert(Location.X >= 0 && Location.X < m_Config.Width);
    assert(Location.Y >= 0 && Location.Y < m_Config.Height);

    // TODO(mkostic): Add support for different sizes of pixels in memory
    const uint32_t* Pixels = (uint32_t*)m_Buffer.data();
    const uint32_t Value = Pixels[Location.X + Location.Y * m_Config.Width];
    Vector4r LinearColor = ColorToVector(Value, m_Config.PixelLayout);
    if (m_Config.GammaSpace == GammaSpace::GammaCorrected)
    {
        LinearColor = ToLinearSpace(LinearColor);
    }
    return LinearColor;
}

rndr::Vector4r rndr::Image::GetPixelColor(int X, int Y) const
{
    return GetPixelColor(Point2i{X, Y});
}

real rndr::Image::GetPixelDepth(const Point2i& Location) const
{
    assert(Location.X >= 0 && Location.X < m_Config.Width);
    assert(Location.Y >= 0 && Location.Y < m_Config.Height);

    const real* Depths = (real*)m_Buffer.data();
    return Depths[Location.X + Location.Y * m_Config.Width];
}

real rndr::Image::GetPixelDepth(int X, int Y) const
{
    return GetPixelDepth(Point2i{X, Y});
}

void rndr::Image::SetPixelColor(const Point2i& Location, const Vector4r& Color)
{
    assert(Location.X >= 0 && Location.X < m_Config.Width);
    assert(Location.Y >= 0 && Location.Y < m_Config.Height);

    uint32_t* Pixels = (uint32_t*)m_Buffer.data();
    Vector4r sRGBColor = Color;
    if (m_Config.GammaSpace == GammaSpace::GammaCorrected)
    {
        sRGBColor = ToGammaCorrectSpace(Color);
    }
    const uint32_t Packed = ColorToUInt32(sRGBColor, m_Config.PixelLayout);
    Pixels[Location.X + Location.Y * m_Config.Width] = Packed;
}

void rndr::Image::SetPixelColor(int X, int Y, const Vector4r& Color)
{
    SetPixelColor(Point2i{X, Y}, Color);
}

void rndr::Image::SetPixelColor(const Point2i& Location, uint32_t Color)
{
    assert(Location.X >= 0 && Location.X < m_Config.Width);
    assert(Location.Y >= 0 && Location.Y < m_Config.Height);

    uint32_t* Pixels = (uint32_t*)m_Buffer.data();
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

    real* Depths = (real*)m_Buffer.data();
    Depths[Location.X + Location.Y * m_Config.Width] = Depth;
}

void rndr::Image::SetPixelDepth(int X, int Y, real Depth)
{
    SetPixelDepth(Point2i{X, Y}, Depth);
}

void rndr::Image::ClearColorBuffer(const Vector4r& Color)
{
    RNDR_CPU_TRACE("Image Clear Color");

    // TODO(mkostic): Add support for different pixel layout sizes
    Vector4r sRGBColor = Color;
    if (m_Config.GammaSpace == GammaSpace::GammaCorrected)
    {
        sRGBColor = ToGammaCorrectSpace(Color);
    }
    const uint32_t PackedColor = ColorToUInt32(sRGBColor, m_Config.PixelLayout);

    ParallelFor(m_Config.Height, 64,
                [this, PackedColor](int RowIndex)
                {
                    uint32_t* Pixels = (uint32_t*)m_Buffer.data() + m_Config.Width * RowIndex;
                    const uint32_t Size = m_Config.Width;
                    for (int i = 0; i < Size; i++)
                    {
                        *Pixels++ = PackedColor;
                    }
                });
}

void rndr::Image::ClearDepthBuffer(real ClearValue)
{
    RNDR_CPU_TRACE("Image Clear Depth");

    ParallelFor(m_Config.Height, 64,
                [this, ClearValue](int RowIndex)
                {
                    real* Pixels = (real*)m_Buffer.data() + m_Config.Width * RowIndex;
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

void rndr::Image::SetPixelFormat(rndr::GammaSpace Space, rndr::PixelLayout Layout)
{
    // TODO(mkostic): Add support for layouts that use more or less then 4 bytes
    assert(rndr::GetPixelSize(Layout) == 4);

    ParallelFor(m_Bounds.Extent(), 64,
                [&](int X, int Y)
                {
                    Vector4r OldColor = GetPixelColor(X, Y);
                    if (m_Config.GammaSpace != Space)
                    {
                        OldColor = ToDesiredSpace(OldColor, Space);
                    }
                    const uint32_t NewColor = ColorToUInt32(OldColor, Layout);
                    assert(NewColor & 0xFF000000);
                    SetPixelColor(X, Y, NewColor);
                });

    m_Config.PixelLayout = Layout;
    m_Config.GammaSpace = Space;
}

void rndr::Image::GenerateMipMaps()
{
    for (int i = 0; i < m_MipMaps.size(); i++)
    {
        if (i != 0)
        {
            delete m_MipMaps[i];
            m_MipMaps[i] = nullptr;
        }
    }

    const int Max = std::max(m_Config.Width, m_Config.Height);
    const int LevelCount = (int)rndr::Log2(Max) + 1;

    m_MipMaps.resize(LevelCount);

    for (int i = 0; i < m_MipMaps.size(); i++)
    {
        if (i == 0)
        {
            m_MipMaps[0] = this;
            continue;
        }

        ImageConfig Config;
        Config.Width = m_MipMaps[i - 1]->m_Config.Width / 2;
        Config.Height = m_MipMaps[i - 1]->m_Config.Height / 2;

        Config.Width = Config.Width == 0 ? 1 : Config.Width;
        Config.Height = Config.Height == 0 ? 1 : Config.Height;

        m_MipMaps[i] = new Image(Config);

        Image* CurrentImage = m_MipMaps[i];
        Image* PrevImage = m_MipMaps[i - 1];

        for (int Y = 0; Y < m_MipMaps[i]->m_Config.Height; Y++)
        {
            for (int X = 0; X < m_MipMaps[i]->m_Config.Width; X++)
            {
                Vector4r Result;
                if (PrevImage->m_Config.Width == 1)
                {
                    const Vector4r Bottom = PrevImage->GetPixelColor(0, 2 * Y);
                    const Vector4r Top = PrevImage->GetPixelColor(0, 2 * Y + 1);
                    Result = 0.5 * Bottom + 0.5 * Top;
                }
                else if (PrevImage->m_Config.Height == 1)
                {
                    const Vector4r Left = PrevImage->GetPixelColor(2 * X, 0);
                    const Vector4r Right = PrevImage->GetPixelColor(2 * X + 1, 0);
                    Result = 0.5 * Left + 0.5 * Right;
                }
                else
                {
                    const Vector4r BottomLeft = PrevImage->GetPixelColor(2 * X, 2 * Y);
                    const Vector4r BottomRight = PrevImage->GetPixelColor(2 * X + 1, 2 * Y);
                    const Vector4r TopLeft = PrevImage->GetPixelColor(2 * X, 2 * Y + 1);
                    const Vector4r TopRight = PrevImage->GetPixelColor(2 * X + 1, 2 * Y + 1);
                    Result = 0.25 * BottomLeft + 0.25 * BottomRight + 0.25 * TopLeft + 0.25 * TopRight;
                }

                CurrentImage->SetPixelColor(X, Y, Result);
            }
        }
    }
}

rndr::Vector4r rndr::Image::Sample(const Point2r& TexCoord, const Vector2r& duvdx, const Vector2r& duvdy)
{
    // TODO(mkostic): Rebase uv to be in range [0, 1]

    const real Width = std::max(std::max(std::abs(duvdx.X), std::abs(duvdx.Y)), std::max(std::abs(duvdy.X), std::abs(duvdy.Y)));

    const int MipMapLevels = m_MipMaps.size();
    real LOD = MipMapLevels - 1 + Log2(std::max(Width, (real)1e-8));
    LOD += m_Config.LODBias;

    const ImageFiltering Filter = LOD < 0 ? m_Config.MagFilter : m_Config.MinFilter;

    Vector4r Result;
    switch (Filter)
    {
        case ImageFiltering::NearestNeighbor:
        {
            Result = SampleNearestNeighbor(this, TexCoord);
            break;
        }
        case ImageFiltering::BilinearInterpolation:
        {
            Result = SampleBilinear(this, TexCoord);
            break;
        }
        case ImageFiltering::TrilinearInterpolation:
        {
            assert(LOD >= 0);  // Not allowed for magnification filters
            Result = SampleTrilinear(this, TexCoord, LOD);
            break;
        }
    }

    return Result;
}

rndr::Vector4r rndr::Image::SampleNearestNeighbor(const Image* I, const Point2r& TexCoord)
{
    const real U = TexCoord.X;
    const real V = TexCoord.Y;

    const real X = (I->m_Config.Width - 1) * U;
    const real Y = (I->m_Config.Height - 1) * V;

    const rndr::Point2i NearestDesc{(int)X, (int)Y};
    return I->GetPixelColor(NearestDesc);
}

rndr::Vector4r rndr::Image::SampleBilinear(const Image* I, const Point2r& TexCoord)
{
    const real U = TexCoord.X;
    const real V = TexCoord.Y;

    const real X = (I->m_Config.Width - 1) * U;
    const real Y = (I->m_Config.Height - 1) * V;

    const Point2i BottomLeft{(int)(X - 0.5), (int)(Y - 0.5)};
    const Point2i BottomRight{(int)(X + 0.5), (int)(Y - 0.5)};
    const Point2i TopLeft{(int)(X - 0.5), (int)(Y + 0.5)};
    const Point2i TopRight{(int)(X + 0.5), (int)(Y + 0.5)};

    Vector4r Result;
    // clang-format off
    Result = I->GetPixelColor(BottomLeft)  * (1 - U) * (1 - V) +
             I->GetPixelColor(BottomRight) *      U  * (1 - V) +
             I->GetPixelColor(TopLeft)     * (1 - U) *      V  +
             I->GetPixelColor(TopRight)    *      U  *      V;
    // clang-format on

    return Result;
}

rndr::Vector4r rndr::Image::SampleTrilinear(const Image* I, const Point2r& TexCoord, real LOD)
{
    const int Floor = (int)LOD;
    int Ceil = (int)(LOD + 1);

    if (Ceil == I->m_MipMaps.size())
    {
        Ceil = Floor;
    }

    const Vector4r FloorSample = SampleBilinear(I->m_MipMaps[Floor], TexCoord);
    const Vector4r CeilSample = SampleBilinear(I->m_MipMaps[Ceil], TexCoord);

    return rndr::Lerp(LOD - (real)Floor, FloorSample, CeilSample);
}

#endif  // RNDR_RASTER
