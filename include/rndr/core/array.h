#pragma once

#include <vector>

#include "rndr/core/definitions.h"

namespace rndr
{

/**
 * A dynamic array allocated on the heap.
 */
template <typename T>
using Array = std::vector<T>;

using ByteArray = Array<uint8_t>;

}  // namespace rndr