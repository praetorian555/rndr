#pragma once

#include <string>

#include "rndr/core/rndr.h"

namespace rndr
{

class Image;

/**
 * Converts value to gamma correct space from linear space. Value^(1 / Gamma).
 *
 * @param Value Value to convert. Must be in range [0, 1].
 * @param Gamma Exponent. Default value is 2.4.
 * @return Returns converted value in range [0, 1].
 */
real ToGammaCorrectSpace(real Value, real Gamma = RNDR_GAMMA);

/**
 * Converts value to linear space from a gamma correct space. Value^(Gamma).
 *
 * @param Value Value to convert. Must be in range [0, 1].
 * @param Gamma Exponent. Default value is 2.4.
 * @return Returns converted value in range [0, 1].
 */
real ToLinearSpace(real Value, real Gamma = RNDR_GAMMA);

/**
 * Get size of a pixel in bytes.
 *
 * @param Layout Specifies the order of channels and their size.
 * @return Returns the size of a pixel in bytes.
 */
int GetPixelSize(PixelLayout Layout);

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