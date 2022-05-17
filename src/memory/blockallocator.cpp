#include "rndr/memory/blockallocator.h"

#if RNDR_WINDOWS
#include <Windows.h>
#endif

#define INVALID_BLOCK_ID (-1)

rndr::BlockAllocator::BlockAllocator(size_t BlockSize, size_t MaxBlocksPerSection, const std::string& DebugName)
    : m_BlockSize(BlockSize), m_BlockCount(MaxBlocksPerSection), m_DebugName(DebugName)
{
    assert(BlockSize >= 8);

    AllocateSection(MaxBlocksPerSection, BlockSize);
}

rndr::BlockAllocator::~BlockAllocator()
{
    for (const Section& S : m_Sections)
    {
#if RNDR_WINDOWS
        BOOL Status = VirtualFree(S.Data, 0, MEM_RELEASE);
        assert(Status);
#else
#error "Platform not supported!"
#endif
    }
}

void* rndr::BlockAllocator::Allocate(size_t Size, size_t Allignment)
{
    Section& S = FindFreeSection();
    assert(S.FirstFreeBlock != INVALID_BLOCK_ID);

    const int BlockToAllocate = S.FirstFreeBlock;
    uint8_t* Ptr = S.Data + S.FirstFreeBlock * S.BlockSize;
    S.FirstFreeBlock = *(int64_t*)Ptr;
    return Ptr;
}

void rndr::BlockAllocator::Free(void* Ptr)
{
    Section& S = FindSection(Ptr);

    // Zero the memory
    memset(Ptr, 0, S.BlockSize);

    int64_t* Data = (int64_t*)Ptr;
    *Data = INVALID_BLOCK_ID;
    int64_t LastBlock = ((uint8_t*)Ptr - S.Data) / S.BlockSize;
    Data = (int64_t*)(S.Data + S.BlockSize * S.LastFreeBlock);
    *Data = LastBlock;
    S.LastFreeBlock = LastBlock;
}

// Reset allocator state
void rndr::BlockAllocator::Reset()
{
    for (Section& S : m_Sections)
    {
        Reset(S);
    }
}

void rndr::BlockAllocator::Reset(Section& S)
{
    for (int i = 0; i < S.BlockCount; i++)
    {
        int64_t* Ptr = (int64_t*)(S.Data + i * S.BlockSize);
        *Ptr = i != S.BlockCount - 1 ? i + 1 : INVALID_BLOCK_ID;
    }
    S.FirstFreeBlock = 0;
    S.LastFreeBlock = S.BlockCount - 1;
}

rndr::BlockAllocator::Section& rndr::BlockAllocator::FindFreeSection()
{
    for (Section& S : m_Sections)
    {
        if (S.FirstFreeBlock != INVALID_BLOCK_ID)
        {
            return S;
        }
    }

    // All sections are full, allocate a new one
    return AllocateSection(m_BlockCount, m_BlockSize);
}

rndr::BlockAllocator::Section& rndr::BlockAllocator::FindSection(void* Ptr)
{
    uint64_t TestVal = (uint64_t)Ptr;
    for (Section& S : m_Sections)
    {
        uint64_t Data = (uint64_t)S.Data;
        if (TestVal >= Data && TestVal < Data + S.BlockSize * S.BlockCount)
        {
            return S;
        }
    }

    assert(false);
    return m_Sections.front();
}

rndr::BlockAllocator::Section& rndr::BlockAllocator::AllocateSection(uint64_t BlockCount, uint64_t BlockSize)
{
    Section S;
    S.BlockCount = BlockCount;
    S.BlockSize = BlockSize;

#if RNDR_WINDOWS
    void* Addr = VirtualAlloc(nullptr, S.BlockSize * S.BlockCount, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    assert(Addr);
    S.Data = reinterpret_cast<uint8_t*>(Addr);
#else
#error "Platform not supported!"
#endif

    Reset(S);

    m_Sections.push_back(S);

    return m_Sections.back();
}
