#pragma once
#include <string>

#include "rndr/core/base.h"
#include "rndr/core/pipeline.h"

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

}  // namespace rndr