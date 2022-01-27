#include "utilities/bmpparser.h"

#if RNDR_WINDOWS
#include "Windows.h"
#endif  // RNDR_WINDOWS

#include "rndr/core/color.h"

#include "rndr/render/image.h"

namespace rndr
{

static constexpr int BitmapInfoSize = 40;
static constexpr int BitmapInfoV4Size = 108;
static constexpr int BitmapInfoV5Size = 124;

struct BitmapFileInfo
{
    uint16_t Type;
    uint32_t Size;
    uint32_t OffBits;
};

struct CIEXYZ
{
    int32_t X;
    int32_t Y;
    int32_t Z;
};

struct CIEXYZTriple
{
    CIEXYZ Red;
    CIEXYZ Green;
    CIEXYZ Blue;
};

struct BitmapInfo
{
    uint32_t StructSize;
    int32_t Width;
    int32_t Height;
    uint16_t Planes;
    uint16_t BitCount;
    uint32_t Compression;
    uint32_t SizeImage;
    int32_t XPixelsPerMeter;
    int32_t YPixelsPerMeter;
    uint32_t ColorUsedNb;
    uint32_t ColorImportantNb;
    uint32_t RedMask;
    uint32_t GreenMask;
    uint32_t BlueMask;
    uint32_t AlphaMask;
    uint32_t ColorSpaceType;
    CIEXYZTriple Endpoints;
    uint32_t GammaRed;
    uint32_t GammaGreen;
    uint32_t GammaBlue;
    uint32_t Intent;
    uint32_t ProfileData;
    uint32_t ProfileSize;
};

#if RNDR_LITTLE_ENDIAN

template <typename T>
T ReadValue(uint8_t* Ptr)
{
    T* Tmp = (T*)Ptr;
    return *Tmp;
}

#elif RNDR_BIG_ENDIAN

template <typename T>
T ReadValue(uint8_t* Ptr)
{
    T Value;
    uint8_t* Tmp = (uint8_t*)Value;
    const int TypeSize = sizeof(T);
    for (int i = 0; i < TypeSize; i++)
    {
        Tmp[i] = Ptr[TypeSize - i - 1];
    }
    return Value;
}

#endif  // RNDR_LITTLE_ENDIAN

template <typename T>
T ReadAndMove(uint8_t** Ptr)
{
    T* Tmp = (T*)*Ptr;
    T Value = ReadValue<T>(*Ptr);
    Tmp++;
    *Ptr = (uint8_t*)Tmp;
    return Value;
}

static BitmapFileInfo ParseBitmapFileInfo(uint8_t** Ptr)
{
    BitmapFileInfo Info;

    Info.Type = ReadAndMove<uint16_t>(Ptr);
    assert(Info.Type == 0x4D42);
    Info.Size = ReadAndMove<uint32_t>(Ptr);
    ReadAndMove<uint32_t>(Ptr);  // Reserved stuff
    Info.OffBits = ReadAndMove<uint32_t>(Ptr);
    return Info;
}

static BitmapInfo ParseBitmapInfo(uint8_t** Ptr)
{
    BitmapInfo Info;

    Info.StructSize = ReadAndMove<uint32_t>(Ptr);
    assert(Info.StructSize == BitmapInfoSize || Info.StructSize == BitmapInfoV4Size ||
           Info.StructSize == BitmapInfoV5Size);
    Info.Width = ReadAndMove<int32_t>(Ptr);
    Info.Height = ReadAndMove<int32_t>(Ptr);
    Info.Planes = ReadAndMove<uint16_t>(Ptr);
    Info.BitCount = ReadAndMove<uint16_t>(Ptr);
    Info.Compression = ReadAndMove<uint32_t>(Ptr);
    Info.SizeImage = ReadAndMove<uint32_t>(Ptr);
    Info.XPixelsPerMeter = ReadAndMove<int32_t>(Ptr);
    Info.YPixelsPerMeter = ReadAndMove<int32_t>(Ptr);
    Info.ColorUsedNb = ReadAndMove<uint32_t>(Ptr);
    Info.ColorImportantNb = ReadAndMove<uint32_t>(Ptr);

    if (Info.StructSize == BitmapInfoV4Size || Info.StructSize == BitmapInfoV5Size)
    {
        Info.RedMask = ReadAndMove<uint32_t>(Ptr);
        Info.GreenMask = ReadAndMove<uint32_t>(Ptr);
        Info.BlueMask = ReadAndMove<uint32_t>(Ptr);
        Info.AlphaMask = ReadAndMove<uint32_t>(Ptr);
        Info.ColorSpaceType = ReadAndMove<uint32_t>(Ptr);
        Info.Endpoints = ReadAndMove<CIEXYZTriple>(Ptr);
        Info.GammaRed = ReadAndMove<uint32_t>(Ptr);
        Info.GammaGreen = ReadAndMove<uint32_t>(Ptr);
        Info.GammaBlue = ReadAndMove<uint32_t>(Ptr);
    }

    if (Info.StructSize == BitmapInfoV5Size)
    {
        Info.Intent = ReadAndMove<uint32_t>(Ptr);
        Info.ProfileData = ReadAndMove<uint32_t>(Ptr);
        Info.ProfileSize = ReadAndMove<uint32_t>(Ptr);
    }

    return Info;
}

static std::vector<Color> ParseColorPallete(uint8_t** Ptr, const BitmapInfo& Info)
{
    int ColorTableSize = 0;
    switch (Info.BitCount)
    {
        case 1:
        {
            // Monochrome image, two colors only
            ColorTableSize = 2;
            break;
        }
        case 4:
        case 8:
        {
            const int MaxClrCount = Info.BitCount == 4 ? 16 : 256;
            ColorTableSize = Info.ColorUsedNb == 0 ? MaxClrCount : Info.ColorUsedNb;
            break;
        }
    }

    std::vector<Color> ColorTable;
    if (ColorTableSize > 0)
    {
        ColorTable.resize(ColorTableSize);

        for (int i = 0; i < ColorTableSize; i++)
        {
            // TODO(mkostic): Check if any color space calculation is needed
            const uint32_t Blue = ReadAndMove<uint8_t>(Ptr);
            const uint32_t Green = ReadAndMove<uint8_t>(Ptr);
            const uint32_t Red = ReadAndMove<uint8_t>(Ptr);
            ReadAndMove<uint8_t>(Ptr);

            const bool bIsPremul = false;
            ColorTable[i] = Color(Red / 255.0, Green / 255.0f, Blue / 255.0, 1,
                                  rndr::GammaSpace::GammaCorrected, bIsPremul);
        }
    }

    return ColorTable;
}

static uint8_t* ParseImageData(uint8_t* Ptr,
                               const BitmapInfo& Info,
                               const std::vector<Color>& ColorPallete,
                               rndr::Image* Image)
{
    // TODO(mkostic): Here create image buffer based on the desired format, or use platform default
    // format For now use RGBA and store it as A8R8G8B8.
    uint32_t* ImageData = (uint32_t*)Image->GetBuffer();

    if (Info.BitCount == 8)
    {
        assert(Info.Compression == 0);
    }

    switch (Info.BitCount)
    {
        case 1:
        case 2:
        case 4:
        case 8:
        {
            assert(!ColorPallete.empty());
            assert(Info.Compression == 0);
            const int PixelsPerByte = 8 / Info.BitCount;
            const int BitsPerPixel = Info.BitCount;
            const int ByteCount = Info.Width * Info.Height / PixelsPerByte;

            uint8_t BaseMask = 0;
            for (int i = 0; i < BitsPerPixel; i++)
            {
                BaseMask |= (1 << i);
            }

            for (int i = 0; i < ByteCount; i++, Ptr++)
            {
                for (int j = PixelsPerByte - 1; j >= 0; j--)
                {
                    const int ShiftAmount = BitsPerPixel * j;
                    const uint8_t Mask = BaseMask << BitsPerPixel * j;
                    const int ColorPalleteIndex = (*Ptr & Mask) >> ShiftAmount;
                    const int PixelIndex = i * PixelsPerByte + PixelsPerByte - j - 1;
                    ImageData[PixelIndex] = ColorPallete[ColorPalleteIndex].ToUInt32(
                        rndr::GammaSpace::GammaCorrected, PixelLayout::B8G8R8A8);
                }
            }
            break;
        }
        case 16:
        {
            assert(false);
            break;
        }
        case 24:
        {
            assert(false);
            break;
        }
        case 32:
        {
            assert(false);
            break;
        }
    }

    return (uint8_t*)ImageData;
}

static rndr::PixelLayout ParsePixelLayout(const BitmapInfo& Info)
{
    switch (Info.BitCount)
    {
        case 1:
        case 2:
        case 4:
        case 8:
        {
            return PixelLayout::B8G8R8A8;
        }
        default:
        {
            assert(false);
            return PixelLayout::A8R8G8B8;
        }
    }
}

rndr::Image* rndr::BmpParser::Read(const std::string& FilePath)
{
    FILE* FileHandle = fopen(FilePath.c_str(), "rb");
    assert(FileHandle);

    fseek(FileHandle, 0, SEEK_END);
    uint64_t FileSize = ftell(FileHandle);
    fseek(FileHandle, 0, SEEK_SET);
    assert(FileSize > 0);

    std::vector<uint8_t> ImageBuffer(FileSize);

    fread(ImageBuffer.data(), 1, FileSize, FileHandle);

    uint8_t* Copy = ImageBuffer.data();
    uint8_t** Offset = &Copy;
    BitmapFileInfo BitmapFileInfo = ParseBitmapFileInfo(Offset);
    BitmapInfo BitmapInfo = ParseBitmapInfo(Offset);
    const std::vector<Color> ColorPallete = ParseColorPallete(Offset, BitmapInfo);
    assert(*Offset == ImageBuffer.data() + BitmapFileInfo.OffBits);

    rndr::ImageConfig Config;
    Config.Width = BitmapInfo.Width;
    Config.Height = BitmapInfo.Height;
    // TODO(mkostic): We need to figure out pixel format based on BitmapInfo
    Config.GammaSpace = rndr::GammaSpace::GammaCorrected;
    Config.PixelLayout = ParsePixelLayout(BitmapInfo);
    rndr::Image* Image = new rndr::Image(Config);
    uint8_t* ImageData = ParseImageData(*Offset, BitmapInfo, ColorPallete, Image);

    fclose(FileHandle);

    return Image;
}

}  // namespace rndr