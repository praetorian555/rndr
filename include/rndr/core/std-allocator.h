#pragma once

#include "rndr/core/base.h"

namespace rndr
{

/**
 * Helper class representing the allocator API compatible with standard library. It simply wraps
 * calls to the library's allocation API.
 */
template <typename T>
class StdAllocator
{
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    StdAllocator() = default;

    template <class U>
    explicit constexpr StdAllocator(const StdAllocator<U>& Other) noexcept
    {
        RNDR_UNUSED(Other);
    }

    T* allocate(size_t Count)
    {
        void* Addr = rndr::Allocate(Count * sizeof(T), "StdAllocator");
        return static_cast<T*>(Addr);
    }

    void deallocate(T* Ptr, size_t Count)
    {
        RNDR_UNUSED(Count);
        rndr::Free(Ptr);
    }
};

}  // namespace rndr