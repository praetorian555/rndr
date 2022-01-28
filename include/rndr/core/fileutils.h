#pragma once

#include <string>

#include "rndr/core/rndr.h"

namespace rndr
{

class Image;

/**
 * Reads a file contents.
 *
 * @param Path to the file on disk.
 * @return Returns the Image object that stores loaded image data and metadata, or nullptr in case
 * of an error. Caller is responsible for Image object's memory.
 */
Image* ReadImageFile(const std::string& FilePath);

/**
 * Get an image file format based on the filepath.
 *
 * @param FilePath Path to the file. We care about file extension.
 * @return Returns ImageFileFormat enum value. If format is not supported it returns
 * ImageFileFormat::NotSupported.
 */
ImageFileFormat GetImageFileFormat(const std::string& FilePath);

}  // namespace rndr