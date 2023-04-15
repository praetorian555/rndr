#pragma once

#include <functional>

#include "rndr/core/definitions.h"

namespace Rndr
{
/**
 * Wrapper around a raw pointer to the type T. It allows copying of the pointer but no moving.
 */
template <typename T>
using Ref = std::reference_wrapper<T>;
} // namespace Rndr
