#pragma once

#include <vector>

#include "rndr/core/base.h"

#include "rndr/utility/array.h"

namespace rndr
{

template <typename T>
struct Span
{
    Span() : Data(nullptr), Size(0) {}
    Span(T* Data, uint64_t Size) : Data(Data), Size(Size) {}
    explicit Span(const std::vector<T>& Vec)
        : Data(reinterpret_cast<T*>(Vec.data())), Size(Vec.size())
    {
    }

    explicit Span(Array<T>& Vec) : Data(Vec.data()), Size(Vec.size()) {}

    template <typename U>
    explicit Span(std::vector<U>& Vec)
    {
        Data = reinterpret_cast<T*>(Vec.data());
        Size = Vec.size() * sizeof(U) / sizeof(T);
    }

    template <typename U>
    explicit Span(Array<U>& Vec)
    {
        Data = reinterpret_cast<T*>(Vec.data());
        Size = Vec.size() * sizeof(U) / sizeof(T);
    }

    template <typename U>
    explicit Span(const Span<U>& Other)
    {
        Data = reinterpret_cast<T*>(Other.Data);
        Size = Other.Size * sizeof(U) / sizeof(T);
    }

    template <typename U>
    explicit Span(U* Ptr)
    {
        Data = reinterpret_cast<T*>(Ptr);
        Size = sizeof(U) / sizeof(T);
    }

    explicit operator bool() const { return Data != nullptr && Size != 0; }

    T& operator[](int Index) { return Data[Index]; }
    const T& operator[](int Index) const { return Data[Index]; }

    T* Data;
    uint64_t Size;
};

using ByteSpan = Span<uint8_t>;
using IntSpan = Span<int>;

}  // namespace rndr