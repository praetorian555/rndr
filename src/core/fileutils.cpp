#include "rndr/core/fileutils.h"

#include <filesystem>

#include "stb_image/stb_image.h"

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

rndr::CPUImage rndr::ReadEntireImage(const std::string& FilePath)
{
    // We need this since stb loads data in top to bottom fashion
    stbi_set_flip_vertically_on_load(true);

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
