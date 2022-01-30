#include "rndr/core/fileutils.h"

#include <filesystem>

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
