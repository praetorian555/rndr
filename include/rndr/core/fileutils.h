#pragma once
#include <string>

#include "rndr/core/base.h"

#include "rndr/render/graphicstypes.h"

namespace rndr
{

/**
 * Get an image file format based on the filepath.
 *
 * @param FilePath Path to the file. We care about file extension.
 * @return Returns ImageFileFormat enum value. If format is not supported it returns
 * ImageFileFormat::NotSupported.
 */
ImageFileFormat GetImageFileFormat(const std::string& FilePath);

ByteSpan ReadEntireFile(const std::string& FilePath);

struct CPUImage
{
    int Width = 0;
    int Height = 0;
    PixelFormat Format = PixelFormat::R8G8B8A8_TYPELESS;
    ByteSpan Data;
};

CPUImage ReadEntireImage(const std::string& FilePath);

void FreeImage(const CPUImage& Image);

}  // namespace rndr
