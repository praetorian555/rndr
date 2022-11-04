#pragma once

#include "rndr/core/base.h"

namespace rndr
{

class Allocator
{
public:
    virtual ~Allocator() {}

    virtual void* Allocate(int Size, const char* Tag, const char* File, int Line) = 0;
    virtual void Deallocate(void* Ptr) = 0;
};

class DefaultAllocator : public Allocator
{
    virtual void* Allocate(int Size, const char* Tag, const char* File, int Line) override;
    virtual void Deallocate(void* Ptr) override;
};

}  // namespace rndr
