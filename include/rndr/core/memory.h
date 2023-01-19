#pragma once

#include <string_view>

#include "rndr/core/allocator.h"
#include "rndr/core/base.h"

namespace rndr
{

// Declartion, defined in rndrcontext.cpp
Allocator* GetAllocator();

/**
 * Allocates the memory for an object of type T and invokes his constuctor.
 *
 * @note Which constructor is called depened on the variable list of arguments at the end of the
 * function.
 *
 * @tparam T Type of object to allocate.
 * @tparam Args Variadic list of types representing the data to be passed to the constructor.
 * @param Alloc Allocator to be used to allocate memory.
 * @param Tag String potentially used for debugging purposes.
 * @param Arguments Variadic list of arguments to be forwaded to the constructor.
 *
 * @return Returns the initialized object on success, or nullptr if the allocator is out of memory.
 */
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

/**
 * Invokes the destructor on the Ptr object and deallocates the memory.
 *
 * @tparam T Type of the object to be destroyed.
 * @param Alloc Allocator to be used to deallocate the memory.
 * @param Ptr Object to destroy.
 */
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

/**
 * Allocates the memory for an object of type T and invokes his constuctor. It uses the allocator
 * from global RndrContext object.
 *
 * @note Which constructor is called depened on the variable list of arguments at the end of the
 * function.
 *
 * @tparam T Type of object to allocate.
 * @tparam Args Variadic list of types representing the data to be passed to the constructor.
 * @param Tag String potentially used for debugging purposes.
 * @param Arguments Variadic list of arguments to be forwaded to the constructor.
 *
 * @return Returns the initialized object on success, or nullptr if the allocator is out of memory.
 */
template <typename T, typename... Args>
T* New(std::string_view Tag, Args&&... Arguments)
{
    Allocator* Alloc = GetAllocator();
    assert(Alloc);
    return New<T>(Alloc, Tag, std::forward<Args>(Arguments)...);
}

/**
 * Invokes the destructor on the Ptr object and deallocates the memory. It uses the allocator
 * from global RndrContext object.
 *
 * @tparam T Type of the object to be destroyed.
 * @param Ptr Object to destroy.
 */
template <typename T>
void Delete(T* Ptr)
{
    Allocator* Alloc = GetAllocator();
    assert(Alloc);
    Delete(Alloc, Ptr);
}

/**
 * Helper class that implements the RAII memory management for all objects allocated using the
 * allocator in the global RndrContext object. Similar to std::unique_ptr. Once created ScopePtr
 * can't be copied but it can be moved. It is used to represent the ownership of specific object.
 *
 * @note Use CreateScoped to create objects wrapped in ScopePtr.
 */
template <typename T>
class ScopePtr
{
public:
    ScopePtr() : m_Data(nullptr) {}
    explicit ScopePtr(T* Data) : m_Data(Data) {}
    ~ScopePtr() { Delete(m_Data); }

    ScopePtr(const ScopePtr& Other) = delete;
    ScopePtr<T>& operator=(const ScopePtr& Other) = delete;

    ScopePtr(ScopePtr&& Other) noexcept : m_Data(Other.m_Data) { Other.m_Data = nullptr; }

    ScopePtr<T>& operator=(ScopePtr&& Other) noexcept
    {
        Delete(m_Data);
        m_Data = Other.m_Data;
        Other.m_Data = nullptr;
        return *this;
    }

    T* Get() { return m_Data; }
    T& GetRef() { return *m_Data; }

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

/**
 * Helper function used to create objects using the allocator in the global RndrContext object and
 * wrapping them into ScopePtr.
 *
 * @tparam T Type of object to allocate.
 * @tparam Args Variadic list of types representing the data to be passed to the constructor.
 * @param Tag String potentially used for debugging purposes.
 * @param Arguments Variadic list of arguments to be forwaded to the constructor.
 *
 * @return Returns the ScopePtr object with the initialized object inside in case of a succes.
 * Otherwise it returns an invalid ScopePtr object.
 */
template <typename T, typename... Args>
ScopePtr<T> CreateScoped(std::string_view Tag, Args&&... Arguments)
{
    return ScopePtr<T>{rndr::New<T>(Tag, std::forward<Args>(Arguments)...)};
}

/**
 * Helper class representing the allocator API compatible with standard library. It simply wraps
 * calls to the allocator stored in the global RndrContext object.
 */
template <typename T>
class StandardAllocatorWrapper
{
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    StandardAllocatorWrapper() = default;

    template <class U>
    explicit constexpr StandardAllocatorWrapper(const StandardAllocatorWrapper<U>& Other) noexcept
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
