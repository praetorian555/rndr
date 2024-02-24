#pragma once

#include <memory>

#include "rndr/core/base.h"

namespace Rndr
{

/**
 * Represents a unique pointer to an object of type T. It can't be copied but can be moved.
 */
template <typename T, typename Deleter = std::default_delete<T>>
using ScopePtr = std::unique_ptr<T>;

template <typename T, typename... Args>
RNDR_FORCE_INLINE Rndr::ScopePtr<T> MakeScoped(Args&&... args)
{
    return Rndr::ScopePtr<T>{RNDR_NEW(T, std::forward<Args>(args)...)};
}

}  // namespace Rndr

/**
 * Helper macro to create a new object of type T on the heap and store it in a ScopePtr.
 */
 // TODO: Remove this macro.
#define RNDR_MAKE_SCOPED(type, ...) Rndr::ScopePtr<type>(RNDR_NEW(type, __VA_ARGS__))
