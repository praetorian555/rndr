#pragma once

#include "rndr/core/base.h"
#include "rndr/core/span.h"

namespace rndr
{

// Forward declarations
class Mesh;

struct ObjParser
{
    static Mesh* Parse(const std::string& FilePath);
    static Mesh* Parse(Span<char> Data);
};

}  // namespace rndr