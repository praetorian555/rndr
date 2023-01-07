#pragma once

#include <string_view>

#include "rndr/core/allocator.h"
#include "rndr/core/base.h"

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

Allocator* GetAllocator();
template <typename T>
class ScopePtr
{
public:
    ScopePtr() : m_Data(nullptr) {}
    explicit ScopePtr(T* Data) : m_Data(Data) {}
    ~ScopePtr() { Delete<T>(GetAllocator(), m_Data); }

    ScopePtr(const ScopePtr& Other) = delete;
    ScopePtr<T>& operator=(const ScopePtr& Other) = delete;

    ScopePtr(ScopePtr&& Other) noexcept
    {
        Delete<T>(GetAllocator(), m_Data);
        m_Data = Other.m_Data;
        Other.m_Data = nullptr;
    }

    ScopePtr<T>& operator=(ScopePtr&& Other) noexcept
    {
        Delete<T>(GetAllocator(), m_Data);
        m_Data = Other.m_Data;
        Other.m_Data = nullptr;
        return *this;
    }

    T* Get() { return m_Data; }

    T& operator*() { return *m_Data; }
    T* operator->() { return m_Data; }

    [[nodiscard]] bool IsValid() const { return m_Data != nullptr; }

    void Reset(T* NewData = nullptr)
    {
        Delete<T>(GetAllocator(), m_Data);
        m_Data = NewData;
    }

private:
    T* m_Data = nullptr;
};

template <typename T, typename... Args>
ScopePtr<T> CreateScoped(std::string_view Tag, Args&&... Arguments)
{
    return ScopePtr<T>{rndr::New<T>(rndr::GetAllocator(), Tag.data(), __FILE__, __LINE__,
                                    std::forward<Args>(Arguments)...)};
}

}  // namespace rndr

#define RNDR_NEW(Type, Tag, ...) \
    rndr::New<Type>(rndr::GetAllocator(), Tag, __FILE__, __LINE__, __VA_ARGS__)
#define RNDR_NEW_ARRAY(Type, Count, Tag, ...) \
    rndr::NewArray<Type>(rndr::GetAllocator(), Count, Tag, __FILE__, __LINE__)

#define RNDR_DELETE(Type, Ptr) rndr::Delete<Type>(rndr::GetAllocator(), Ptr), Ptr = nullptr
#define RNDR_DELETE_ARRAY(Type, Ptr, Count) \
    rndr::DeleteArray<Type>(rndr::GetAllocator(), Ptr, Count), Ptr = nullptr
