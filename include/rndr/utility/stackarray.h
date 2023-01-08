#pragma once

#include <array>

#include "rndr/core/base.h"

namespace rndr
{

template <typename T, size_t N>
using StackArray = std::array<T, N>;

}