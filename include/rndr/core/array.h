#pragma once

#include <vector>

#include "rndr/core/std-allocator.h"

namespace rndr
{

/**
 * A dynamic array allocated on the heap.
 */
template <typename T>
using Array = std::vector<T, StdAllocator<T>>;

using ByteArray = Array<uint8_t>;

}  // namespace rndr