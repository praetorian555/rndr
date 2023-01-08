#pragma once

#include <vector>

#include "rndr/core/base.h"
#include "rndr/core/memory.h"

namespace rndr
{

template <typename T>
using Array = std::vector<T, StandardAllocatorWrapper<T>>;

using ByteArray = Array<uint8_t>;

}