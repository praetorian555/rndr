#include "rndr/core/fileutils.h"

#include <filesystem>

#include "stb_image/stb_image.h"

#include "rndr/render/image.h"

rndr::Image* rndr::ReadImageFile(const std::string& FilePath)
{
    const ImageFileFormat FileFormat = GetImageFileFormat(FilePath);
    assert(FileFormat != ImageFileFormat::NotSupported);

    FILE* FileHandle = fopen(FilePath.c_str(), "r");
    assert(FileHandle);

    ImageConfig Config;

    // TODO(mkostic): Currently we assume that all images are in gamma corrected space
    Config.GammaSpace = rndr::GammaSpace::GammaCorrected;

    // TODO(mkostic): Will be different if we start using grayscale images
    Config.PixelLayout = PixelLayout::R8G8B8A8;

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

    Data = stbi_load_from_file(FileHandle, &Width, &Height, &ChannelNumber, DesiredChannelNumber);

    Config.Width = Width;
    Config.Height = Height;

    // TODO(mkostic): How to know if image uses 16 or 8 bits per channel??

    assert(Data);

    // TODO(mkostic): Add support for grayscale images.
    assert(ChannelNumber == 3 || ChannelNumber == 4);

    Image* I = new Image(Config);

    uint8_t* ImageBuffer = I->GetBuffer();
    const int PixelSize = I->GetPixelSize();
    const int BufferSize = Width * Height * PixelSize;

    for (int ByteIndex = 0; ByteIndex < BufferSize; ByteIndex += PixelSize)
    {
        for (int i = 0; i < PixelSize; i++)
        {
            ImageBuffer[ByteIndex + i] = Data[ByteIndex + PixelSize - 1 - i];
        }
    }

    free(Data);
    fclose(FileHandle);

    return I;
}

rndr::ImageFileFormat rndr::GetImageFileFormat(const std::string& FilePathStr)
{
    static const char* SupportedExtensions[] = {".bmp", ".png", ".jpeg"};
    static const int ExtensionCount = sizeof(SupportedExtensions) / sizeof(const char*);

    const std::filesystem::path FilePath(FilePathStr);
    for (int i = 0; i < ExtensionCount; i++)
    {
        if (FilePath.extension().string() == SupportedExtensions[i])
        {
            return (ImageFileFormat)i;
        }
    }

    return ImageFileFormat::NotSupported;
}
