#pragma once

#include "opal/container/dynamic-array.h"

#include "rndr/types.hpp"

namespace Rndr
{

struct Mesh
{
    Opal::StringUtf8 name;
    Opal::DynamicArray<u8> vertices;
    Opal::DynamicArray<u8> indices;
    u32 vertex_size = 0;
    u32 vertex_count = 0;
    u32 index_size = 0;
    u32 index_count = 0;
};

}  // namespace Rndr