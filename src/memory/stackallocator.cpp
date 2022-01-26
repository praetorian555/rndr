#include "rndr/memory/stackallocator.h"

#if RNDR_WINDOWS
#include <Windows.h>
#endif

#include "rndr/core/log.h"

rndr::StackAllocator::StackAllocator(size_t Size)
{
#if RNDR_WINDOWS
    void* Addr = VirtualAlloc(nullptr, Size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    assert(Addr);
    m_Data = reinterpret_cast<uint8_t*>(Addr);
    m_Size = Size;
    m_Offset = 0;
#else
#error "Platform not supported!"
#endif
}

rndr::StackAllocator::~StackAllocator()
{
#if RNDR_WINDOWS
    BOOL Status = VirtualFree(m_Data, 0, MEM_RELEASE);
    assert(Status);
#else
#error "Platform not supported!"
#endif
}

void* rndr::StackAllocator::Allocate(size_t Size, size_t Allignment)
{
    int64_t MemStart = reinterpret_cast<int64_t>(m_Data);
    int64_t FreeMemStart = reinterpret_cast<int64_t>(m_Data) + m_Offset;
    int64_t MemEnd = reinterpret_cast<int64_t>(m_Data) + m_Size;
    while (FreeMemStart % Allignment != 0)
    {
        FreeMemStart++;
    }

    assert(MemEnd - FreeMemStart >= Size);
    m_Offset = FreeMemStart - MemStart + Size;

    return reinterpret_cast<void*>(FreeMemStart);
}

void rndr::StackAllocator::Free(void* Ptr) {}

void rndr::StackAllocator::Reset()
{
    m_Offset = 0;
}