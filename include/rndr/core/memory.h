#pragma once

#include "rndr/core/base.h"
#include "rndr/core/rndrcontext.h"

namespace rndr
{

template <typename T, typename... Args>
T* New(Allocator* Alloc, const char* Tag, const char* File, int Line, Args&&... Arguments)
{
    if (!Alloc)
    {
        return nullptr;
    }
    void* Memory = Alloc->Allocate(sizeof(T), Tag, File, Line);
    if (!Memory)
    {
        return nullptr;
    }
    return new (Memory) T{std::forward<Args>(Arguments)...};
}

template <typename T>
T* NewArray(Allocator* Alloc, int Count, const char* Tag, const char* File, int Line)
{
    if (!Alloc)
    {
        return nullptr;
    }
    if (Count <= 0)
    {
        return nullptr;
    }
    void* Memory = Alloc->Allocate(Count * sizeof(T), Tag, File, Line);
    if (!Memory)
    {
        return nullptr;
    }
    return new (Memory) T[Count]{};
}

template <typename T>
void Delete(Allocator* Alloc, T* Ptr)
{
    if (!Alloc)
    {
        return;
    }
    if (!Ptr)
    {
        return;
    }
    Ptr->~T();
    Alloc->Deallocate(Ptr);
}

template <typename T>
void DeleteArray(Allocator* Alloc, T* Ptr, int Count)
{
    if (!Alloc)
    {
        return;
    }
    if (!Ptr)
    {
        return;
    }
    if (Count > 0)
    {
        T* It = Ptr;
        for (int i = 0; i < Count; i++)
        {
            It->~T();
            It++;
        }
    }
    Alloc->Deallocate(Ptr);
}

}  // namespace rndr

#define RNDR_NEW(Type, Tag, ...) rndr::New<Type>(rndr::GRndrContext->GetAllocator(), Tag, __FILE__, __LINE__, __VA_ARGS__)
#define RNDR_NEW_ARRAY(Type, Count, Tag, ...) rndr::NewArray<Type>(rndr::GRndrContext->GetAllocator(), Count, Tag, __FILE__, __LINE__)

#define RNDR_DELETE(Type, Ptr) rndr::Delete<Type>(rndr::GRndrContext->GetAllocator(), Ptr), Ptr = nullptr
#define RNDR_DELETE_ARRAY(Type, Ptr, Count) rndr::DeleteArray<Type>(rndr::GRndrContext->GetAllocator(), Ptr, Count), Ptr = nullptr
