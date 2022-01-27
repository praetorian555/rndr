#pragma once

namespace rndr
{

#define RNDR_KB(x) (x * 1024)
#define RNDR_MB(x) (x * 1024 * 1024)
#define RNDR_GB(x) (x * 1024 * 1024 * 1024)

struct Allocator
{
    virtual ~Allocator() = default;

    virtual void* Allocate(size_t Size, size_t Allignment) = 0;
    virtual void Free(void* Ptr) = 0;

    // Reset allocator state
    virtual void Reset() = 0;
};

}  // namespace rndr