#pragma once

#include "opal/container/span.h"

#include "rndr/core/containers/array.h"

namespace Rndr
{

/**
 * @brief A span is a view into a contiguous sequence of objects.
 */
template <typename T>
using Span = Opal::Span<T>;

using ByteSpan = Span<uint8_t>;
using ConstByteSpan = Span<const uint8_t>;
using IntSpan = Span<int>;

template <typename T>
ByteSpan ToByteSpan(T& object)
{
    return {reinterpret_cast<uint8_t*>(&object), sizeof(T)};
}

template <typename T>
ConstByteSpan ToConstByteSpan(T& object)
{
    return {reinterpret_cast<const uint8_t*>(&object), sizeof(T)};
}

template <typename Container>
    requires Opal::Range<Container>
ByteSpan ToByteSpan(Container& container)
{
    auto data = &(*container.begin());
    Opal::u64 size = sizeof(typename Container::value_type) * (container.end() - container.begin());
    return {reinterpret_cast<uint8_t*>(data), size};
}

template <typename Container>
    requires Opal::Range<Container>
ConstByteSpan ToConstByteSpan(Container& container)
{
    auto data = &(*container.begin());
    Opal::u64 size = sizeof(typename Container::value_type) * (container.end() - container.begin());
    return {reinterpret_cast<const uint8_t*>(data), size};
}

}  // namespace Rndr
