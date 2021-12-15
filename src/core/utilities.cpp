#include "rndr/core/utilities.h"

#include <cmath>
#include <filesystem>

#include "rndr/render/image.h"

// Private stuff
#include "utilities/bmpparser.h"

real rndr::ToGammaCorrectSpace(real Value, real Gamma)
{
    if (Value <= 0.0031308)
    {
        return Value * 12.92;
    }

#if !defined(RNDR_REAL_AS_DOUBLE)
    return 1.055 * std::powf(Value, 1 / Gamma) - 0.055;
#else
    return 1.055 * std::pow(Value, 1 / Gamma) - 0.055;
#endif
}

real rndr::ToLinearSpace(real Value, real Gamma)
{
    if (Value <= 0.04045)
    {
        return Value / 12.92;
    }

    real Tmp = (Value + 0.055) / 1.055;

#if !defined(RNDR_REAL_AS_DOUBLE)
    return std::powf(Tmp, Gamma);
#else
    return std::pow(Tmp, Gamma);
#endif
}

int rndr::GetPixelSize(PixelLayout Layout)
{
    switch (Layout)
    {
        case PixelLayout::A8R8G8B8:
        {
            return 4;
        }
        default:
        {
            assert(false);
        }
    }

    return 0;
}

rndr::Image* rndr::ReadImageFile(const std::string& FilePath)
{
    const ImageFileFormat FileFormat = GetImageFileFormat(FilePath);
    assert(FileFormat != ImageFileFormat::NotSupported);

    switch (FileFormat)
    {
        case ImageFileFormat::BMP:
        {
            return BmpParser::Read(FilePath);
        }
        default:
        {
            assert(false);
        }
    }

    return nullptr;
}

rndr::ImageFileFormat rndr::GetImageFileFormat(const std::string& FilePathStr)
{
    static const char* SupportedExtensions[] = {".bmp"};
    static const int ExtensionCount = sizeof(SupportedExtensions) / sizeof(const char*);

    const std::filesystem::path FilePath(FilePathStr);
    for (int i = 0; i < ExtensionCount; i++)
    {
        if (FilePath.stem().string() == SupportedExtensions[i])
        {
            return (ImageFileFormat)i;
        }
    }

    return ImageFileFormat::NotSupported;
}
