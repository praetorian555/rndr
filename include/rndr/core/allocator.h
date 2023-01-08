#pragma once

#include <string_view>

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

    virtual void* Allocate(int Size, std::string_view Tag) = 0;
    virtual void Deallocate(void* Ptr) = 0;
};

class DefaultAllocator : public Allocator
{
public:
    DefaultAllocator() = default;
    ~DefaultAllocator() final = default;

    void* Allocate(int Size, std::string_view Tag) override;
    void Deallocate(void* Ptr) override;
};

}  // namespace rndr
