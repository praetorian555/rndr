#pragma once

#include <string_view>

#include "rndr/core/allocator.h"
#include "rndr/core/base.h"

namespace rndr
{

// Decalartion
Allocator* GetAllocator();

template <typename T, typename... Args>
T* New(Allocator* Alloc, std::string_view Tag, Args&&... Arguments)
{
    if (!Alloc)
    {
        return nullptr;
    }
    void* Memory = Alloc->Allocate(sizeof(T), Tag);
    if (!Memory)
    {
        return nullptr;
    }
    return new (Memory) T{std::forward<Args>(Arguments)...};
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

template <typename T, typename... Args>
T* New(std::string_view Tag, Args&&... Arguments)
{
    Allocator* Alloc = GetAllocator();
    assert(Alloc);
    return New<T>(Alloc, Tag, std::forward<Args>(Arguments)...);
}

template <typename T>
void Delete(T* Ptr)
{
    Allocator* Alloc = GetAllocator();
    assert(Alloc);
    Delete(Alloc, Ptr);
}

template <typename T>
class ScopePtr
{
public:
    ScopePtr() : m_Data(nullptr) {}
    explicit ScopePtr(T* Data) : m_Data(Data) {}
    ~ScopePtr() { Delete(m_Data); }

    ScopePtr(const ScopePtr& Other) = delete;
    ScopePtr<T>& operator=(const ScopePtr& Other) = delete;

    ScopePtr(ScopePtr&& Other) noexcept
    {
        Delete(m_Data);
        m_Data = Other.m_Data;
        Other.m_Data = nullptr;
    }

    ScopePtr<T>& operator=(ScopePtr&& Other) noexcept
    {
        Delete(m_Data);
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
        Delete(m_Data);
        m_Data = NewData;
    }

private:
    T* m_Data = nullptr;
};

template <typename T, typename... Args>
ScopePtr<T> CreateScoped(std::string_view Tag, Args&&... Arguments)
{
    return ScopePtr<T>{rndr::New<T>(Tag, std::forward<Args>(Arguments)...)};
}

template <typename T>
class StandardAllocatorWrapper
{
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    StandardAllocatorWrapper() = default;

    template <class U>
    constexpr StandardAllocatorWrapper(const StandardAllocatorWrapper<U>& Other) noexcept
    {
        RNDR_UNUSED(Other);
    }

    T* allocate(size_t Count)
    {
        Allocator* Alloc = rndr::GetAllocator();
        void* Addr =
            Alloc->Allocate(static_cast<int>(Count) * sizeof(T), "StandardAllocatorWrapper");
        return static_cast<T*>(Addr);
    }

    void deallocate(T* Ptr, size_t Count)
    {
        RNDR_UNUSED(Count);
        Allocator* Alloc = rndr::GetAllocator();
        Alloc->Deallocate(Ptr);
    }
};

}  // namespace rndr
