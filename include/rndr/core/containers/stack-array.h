#pragma once

#include <array>

#include "rndr/core/base.h"

namespace Rndr
{

/**
 * A fixed-size array allocated on the stack.
 */
template <typename T, size_t N>
using StackArray = std::array<T, N>;

}  // namespace Rndr
