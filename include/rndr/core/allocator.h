#pragma once

#include "rndr/core/base.h"

namespace rndr
{

class Allocator
{
public:
    Allocator() = default;
    virtual ~Allocator() = default;

    Allocator(const Allocator& Other) = delete;
    Allocator& operator=(const Allocator& Other) = delete;

    Allocator(Allocator&& Other) = delete;
    Allocator& operator=(Allocator&& Other) = delete;

    virtual void* Allocate(int Size, const char* Tag, const char* File, int Line) = 0;
    virtual void Deallocate(void* Ptr) = 0;
};

class DefaultAllocator : public Allocator
{
public:
    DefaultAllocator() = default;
    ~DefaultAllocator() final = default;

    void* Allocate(int Size, const char* Tag, const char* File, int Line) override;
    void Deallocate(void* Ptr) override;
};

}  // namespace rndr
