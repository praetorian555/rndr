#pragma once

#include "opal/container/array.h"

#include "rndr/core/definitions.h"

// disable warnings on extern before template instantiation
#pragma warning(disable : 4231)

#if defined(RNDR_WINDOWS)
#if rndr_EXPORTS
#define RNDR_EXTERN_TEMPLATE
#else
#define RNDR_EXTERN_TEMPLATE extern
#endif
#endif

namespace Rndr
{

/**
 * A dynamic array allocated on the heap.
 */
template <typename T>
using Array = Opal::Array<T>;

using ByteArray = Array<uint8_t>;

}  // namespace Rndr