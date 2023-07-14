#pragma once

#include <span>

#include "rndr/core/array.h"

namespace Rndr
{

/**
 * @brief A span is a view into a contiguous sequence of objects.
 */
template <typename T>
using Span = std::span<T>;

using ByteSpan = Span<uint8_t>;
using ConstByteSpan = Span<const uint8_t>;
using IntSpan = Span<int>;

template <typename T>
ByteSpan ToByteSpan(T& object)
{
    return {reinterpret_cast<uint8_t*>(&object), sizeof(T)};
}

template <typename T>
ConstByteSpan ToByteSpan(const T& object)
{
    return {reinterpret_cast<const uint8_t*>(&object), sizeof(T)};
}

template <typename T>
ByteSpan ToByteSpan(Array<T>& array)
{
    return {reinterpret_cast<uint8_t*>(array.data()), sizeof(T) * array.size()};
}

template <typename T>
ConstByteSpan ToByteSpan(const Array<T>& array)
{
    return {reinterpret_cast<const uint8_t*>(array.data()), sizeof(T) * array.size()};
}

}  // namespace Rndr
