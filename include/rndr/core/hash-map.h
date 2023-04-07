#pragma once

#include <unordered_map>

#include "rndr/core/std-allocator.h"

namespace rndr
{
/**
 * A hash map allocated on the heap.
 * @tparam Key The key type.
 * @tparam Value The value type.
 * @tparam Hash The hash function.
 */
template <typename Key, typename Value, typename Hash>
using HashMap = std::
    unordered_map<Key, Value, Hash, std::equal_to<Key>, StdAllocator<std::pair<const Key, Value>>>;
}  // namespace rndr
