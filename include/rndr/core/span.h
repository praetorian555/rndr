#pragma once

#include <span>

#include "rndr/core/definitions.h"

namespace rndr
{

/**
 * @brief A span is a view into a contiguous sequence of objects.
 */
template <typename T>
using Span = std::span<T>;

using ByteSpan = Span<uint8_t>;
using IntSpan = Span<int>;

}  // namespace rndr