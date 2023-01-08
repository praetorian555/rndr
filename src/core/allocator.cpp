#include "rndr/core/allocator.h"

#include "rndr/core/base.h"

void* rndr::DefaultAllocator::Allocate(int Size, std::string_view Tag)
{
    RNDR_UNUSED(Tag);

#if RNDR_WINDOWS
    constexpr int kAlignmentInBytes = 16;
    return _aligned_malloc(Size, kAlignmentInBytes);
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
