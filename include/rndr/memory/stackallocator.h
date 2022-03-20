#pragma once

#include "rndr/core/base.h"

#include "rndr/memory/allocator.h"

namespace rndr
{

class StackAllocator : public Allocator
{
public:
    StackAllocator(size_t TotalSize);
    ~StackAllocator();

    virtual void* Allocate(size_t Size, size_t Allignment) override;
    virtual void Free(void* Ptr) override;

    // Reset allocator state
    virtual void Reset() override;

private:
    uint8_t* m_Data;
    uint64_t m_Size;
    uint64_t m_Offset;
};

}  // namespace rndr