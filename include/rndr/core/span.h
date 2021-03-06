#pragma once

#include <vector>

#include "rndr/core/base.h"

namespace rndr
{

template <typename T>
struct Span
{
    Span() : Data(nullptr), Size(0) {}
    Span(T* Data, size_t Size) : Data(Data), Size(Size) {}
    explicit Span(const std::vector<T>& Vec) : Data((T*)Vec.data()), Size(Vec.size()) {}

    template <typename U>
    explicit Span(const std::vector<U>& Vec)
    {
        Data = (T*)Vec.data();
        Size = Vec.size() * sizeof(U) / sizeof(T);
        assert(Vec.size() * sizeof(U) % sizeof(T) == 0);
    }

    template <typename U>
    explicit Span(const Span<U>& Other)
    {
        Data = (T*)Other.Data;
        Size = Other.Size * sizeof(U) / sizeof(T);
        assert(Other.Size * sizeof(U) % sizeof(T) == 0);
    }

    template <typename U>
    explicit Span(const U* Ptr)
    {
        Data = (T*)Ptr;
        Size = sizeof(U) / sizeof(T);
        assert(sizeof(U) % sizeof(T) == 0);
    }

    operator bool() const { return Data != nullptr && Size != 0; }

    T& operator[](int Index) { return Data[Index]; }
    const T& operator[](int Index) const { return Data[Index]; }

    T* Data;
    size_t Size;
};

using ByteSpan = Span<uint8_t>;
using IntSpan = Span<int>;

}  // namespace rndr