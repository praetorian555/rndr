#include "rndr/core/allocator.h"

#include "rndr/core/base.h"

void* rndr::DefaultAllocator::Allocate(int Size, const char* Tag, const char* File, int Line)
{
#if RNDR_WINDOWS
    return _aligned_malloc(Size, 16);
#else
#error "Platform missing default allocator implementation!"
    return nullptr;
#endif
}

void rndr::DefaultAllocator::Deallocate(void* Ptr)
{
#if RNDR_WINDOWS
    _aligned_free(Ptr);
#else
#error "Platform missing default allocator implementation!"
#endif
}
