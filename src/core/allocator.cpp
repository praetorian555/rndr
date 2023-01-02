#include "rndr/core/allocator.h"

#include "rndr/core/base.h"

void* rndr::DefaultAllocator::Allocate(int Size, const char* Tag, const char* File, int Line)
{
    RNDR_UNUSED(Tag);
    RNDR_UNUSED(File);
    RNDR_UNUSED(Line);

#if RNDR_WINDOWS
    constexpr int AlignmentInBytes = 16;
    return _aligned_malloc(Size, AlignmentInBytes);
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
