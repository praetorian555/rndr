#pragma once

#include <unordered_map>
#include <unordered_set>

#include "rndr/core/definitions.h"

namespace Rndr
{
/**
 * A hash map allocated on the heap.
 * @tparam Key The key type.
 * @tparam Value The value type.
 * @tparam Hash The hash function.
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>>
using HashMap = std::unordered_map<Key, Value, Hash>;

template <typename Key, typename Hash = std::hash<Key>>
using HashSet = std::unordered_set<Key, Hash>;

}  // namespace Rndr
