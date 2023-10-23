#pragma once

#include <memory>

#include "rndr/core/base.h"

namespace Rndr
{

/**
 * Represents a unique pointer to an object of type T. It can't be copied but can be moved.
 */
template <typename T>
using ScopePtr = std::unique_ptr<T>;

}  // namespace Rndr

/**
 * Helper macro to create a new object of type T on the heap and store it in a ScopePtr.
 */
#define RNDR_MAKE_SCOPED(type, ...) Rndr::ScopePtr<type>(RNDR_NEW(type, __VA_ARGS__))
