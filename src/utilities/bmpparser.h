#pragma once

#include <string>

#include "rndr/core/rndr.h"

namespace rndr
{

class Image;

/**
 * Helper class used to read and write bitmap files.
 */
struct BmpParser
{
    static Image* Read(const std::string& FilePath);
};

}  // namespace rndr