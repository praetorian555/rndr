#pragma once

#include <list>
#include <string>

#include "rndr/core/rndr.h"

#include "rndr/memory/allocator.h"

namespace rndr
{

struct BlockAllocator : public Allocator
{
    BlockAllocator(size_t BlockSize, size_t MaxBlocksPerSection = 128, const std::string& DebugName = "BlockAllocator");
    ~BlockAllocator();

    virtual void* Allocate(size_t Size, size_t Allignment) override;
    virtual void Free(void* Ptr) override;

    // Reset allocator state
    virtual void Reset() override;

private:
    struct Section
    {
        uint8_t* Data = nullptr;
        uint64_t BlockSize = 0;
        uint64_t BlockCount = 0;

        int64_t FirstFreeBlock = 0;
        int64_t LastFreeBlock = 0;
    };

private:
    void Reset(Section& S);
    Section& FindFreeSection();
    Section& FindSection(void* Ptr);
    Section& AllocateSection(uint64_t BlockCount, uint64_t BlockSize);

private:
    std::list<Section> m_Sections;

    uint64_t m_BlockSize;
    uint64_t m_BlockCount;
    std::string m_DebugName;
};

}  // namespace rndr