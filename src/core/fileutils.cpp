#include "rndr/core/fileutils.h"

#include <filesystem>

#include "stb_image/stb_image.h"

#include "rndr/core/log.h"

rndr::ImageFileFormat rndr::GetImageFileFormat(const std::string& FilePathStr)
{
    static const char* SupportedExtensions[] = {".bmp", ".png", ".jpg"};
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

rndr::ByteSpan rndr::ReadEntireFile(const std::string& FilePath)
{
    FILE* File = fopen(FilePath.c_str(), "rb");
    if (!File)
    {
        RNDR_LOG_ERROR("Failed to open file %s", FilePath.c_str());
        return ByteSpan{};
    }

    fseek(File, 0, SEEK_END);
    int ContentsSize = ftell(File);
    fseek(File, 0, SEEK_SET);

    ByteSpan Contents;
    Contents.Size = ContentsSize;
    Contents.Data = new uint8_t[ContentsSize];
    int ReadBytes = fread(Contents.Data, 1, Contents.Size, File);
    assert(ReadBytes == ContentsSize);

    fclose(File);

    return Contents;
}

rndr::CPUImage rndr::ReadEntireImage(const std::string& FilePath)
{
    int ChannelsInFile;
    CPUImage Image;
    Image.Format = PixelFormat::R8G8B8A8_UNORM_SRGB;
    Image.Data.Data = stbi_load(FilePath.c_str(), &Image.Width, &Image.Height, &ChannelsInFile, 4);
    Image.Data.Size = 4 * Image.Width * Image.Height;

    return Image;
}

void rndr::FreeImage(const CPUImage& Image)
{
    stbi_image_free(Image.Data.Data);
}
