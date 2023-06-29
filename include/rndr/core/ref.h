#pragma once

#include "rndr/core/definitions.h"

namespace Rndr
{
/**
 * Wrapper around a raw pointer to the type T. It does no memory management. It is used to
 * represent a reference to an object of type T and to avoid implementation of copy and move when
 * we need to store raw pointer.
 */
template <typename T>
class Ref
{
public:
    Ref() = default;
    ~Ref() = default;

    Ref(const Ref& other) = default;
    Ref& operator=(const Ref& other) = default;

    Ref(Ref&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
    Ref& operator=(Ref&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        return *this;
    }

    explicit Ref(T* ptr) : m_ptr(ptr) {}
    explicit Ref(T& ref) : m_ptr(&ref) {}

    Ref& operator=(T* ptr)
    {
        m_ptr = ptr;
        return *this;
    }
    Ref& operator=(T& ref)
    {
        m_ptr = &ref;
        return *this;
    }

    [[nodiscard]] bool IsValid() const { return m_ptr != nullptr; }
    [[nodiscard]] T& Get() const { return *m_ptr; }
    [[nodiscard]] T* GetPtr() const { return m_ptr; }

    explicit operator bool() const { return IsValid(); }
    T& operator*() const { return Get(); }
    T* operator->() const { return GetPtr(); }
    operator T&() const { return Get(); }

private:
    T* m_ptr = nullptr;
};

}  // namespace Rndr
