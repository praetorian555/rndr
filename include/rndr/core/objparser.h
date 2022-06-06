#pragma once

#include "rndr/core/base.h"
#include "rndr/core/span.h"

namespace rndr
{

// Forward declarations
class Model;

struct ObjParser
{
    static Model* Parse(const std::string& FilePath);
    static Model* Parse(Span<char> Data);
};

}  // namespace rndr