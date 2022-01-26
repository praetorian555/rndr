#pragma once

#include <map>
#include <mutex>
#include <thread>

#include "rndr/core/rndr.h"

#include "rndr/memory/allocator.h"
#include "rndr/memory/blockallocator.h"

namespace rndr
{

class MemoryManager;

// Macros to used for allocation and constructor invokation ///////////////////////////////////////

#define RNDR_DEFAULT_ALLIGNMENT (64)

template <typename T, typename... Args>
T* New(rndr::Allocator* A, int Count, Args&&... args)
{
    T* Addr = (T*)A->Allocate(Count * sizeof(T), RNDR_DEFAULT_ALLIGNMENT);
    T* Ptr = Addr;
    for (int i = 0; i < Count; i++)
    {
        new (Ptr++) T{std::forward<Args>(args)...};
    }
    return Addr;
}

template <typename T>
void Delete(rndr::Allocator* A, T* DataPtr, int Count)
{
    T* Ptr = DataPtr;
    for (int i = 0; i < Count; i++)
    {
        Ptr->~T();
        Ptr++;
    }

    A->Free(DataPtr);
}

/**
 * Allocates and initializes Count objects of type Type using the AllocatorPtr allocator.
 */
#define RNDR_NEW(AllocatorPtr, Type, Count, ...) rndr::New<Type>(AllocatorPtr, Count, __VA_ARGS__)

/**
 * Allocates memory using persistent allocator and invokes constructor of type Type on this memory.
 */
#define RNDR_NEW_PERSISTENT(Type, Count, ...) \
    RNDR_NEW(rndr::MemoryManager::Get()->GetPersistentAllocator(), Type, Count, __VA_ARGS__)

/**
 * Allocates memory using frame allocator for current thread and invokes constructor of type Type on
 * this memory.
 */
#define RNDR_NEW_FRAME(Type, Count, ...)                                                      \
    RNDR_NEW(rndr::MemoryManager::Get()->GetFrameAllocator(std::this_thread::get_id()), Type, \
             Count, __VA_ARGS__)

/**
 * Allocates memory using frame allocator for game thread and invokes constructor of type Type on
 * this memory.
 */
#define RNDR_NEW_GAME_FRAME(Type, Count, ...) \
    RNDR_NEW(rndr::MemoryManager::Get()->GetGameFrameAllocator(), Type, Count, __VA_ARGS__)

// Macros to used for deallocation and destructor invokation ///////////////////////////////////////

#define RNDR_DELETE(AllocatorPtr, Type, DataPtr, Count) \
    rndr::Delete<Type>(AllocatorPtr, DataPtr, Count)

#define RNDR_DELETE_PERSISTENT(Type, DataPtr, Count) \
    RNDR_DELETE(rndr::MemoryManager::Get()->GetPersistentAllocator(), Type, DataPtr, Count)

#define RNDR_DELETE_FRAME(Type, DataPtr, Count)                                                  \
    RNDR_DELETE(rndr::MemoryManager::Get()->GetFrameAllocator(std::this_thread::get_id()), Type, \
                DataPtr, Count)

#define RNDR_DELETE_GAME_FRAME(Type, DataPtr, Count) \
    RNDR_DELETE(rndr::MemoryManager::Get()->GetGameFrameAllocator(), Type, DataPtr, Count)

// STD Allocators and Deleters ////////////////////////////////////////////////////////////////////

template <typename T>
struct StdPersistentDeleter
{
    void operator()(T* Ptr) { RNDR_DELETE_PERSISTENT(T, Ptr, 1); }
};

template <typename T>
struct StdFrameDeleter
{
    void operator()(T* Ptr) { RNDR_DELETE_FRAME(T, Ptr, 1); }
};

template <typename T>
struct StdGameFrameDeleter
{
    void operator()(T* Ptr) { RNDR_DELETE_GAME_FRAME(T, Ptr, 1); }
};

template <typename T>
struct StdPersistentAllocator
{
    using value_type = T;

    StdPersistentAllocator() = default;
    template <class U>
    constexpr StdPersistentAllocator(const StdPersistentAllocator<U>&) noexcept
    {
    }

    [[nodiscard]] T* allocate(std::size_t n) { return RNDR_NEW_PERSISTENT(T, n); }

    void deallocate(T* p, std::size_t n) { RNDR_DELETE_PERSISTENT(T, p, n); }
};

template <typename T>
struct StdFrameAllocator
{
    using value_type = T;

    StdFrameAllocator() = default;
    template <class U>
    constexpr StdFrameAllocator(const StdFrameAllocator<U>&) noexcept
    {
    }

    [[nodiscard]] T* allocate(std::size_t n) { return RNDR_NEW_FRAME(T, n); }

    void deallocate(T* p, std::size_t n) { RNDR_DELETE_FRAME(T, p, n); }
};

template <typename T>
struct StdGameFrameAllocator
{
    using value_type = T;

    StdGameFrameAllocator() = default;

    template <class U>
    constexpr StdGameFrameAllocator(const StdGameFrameAllocator<U>&) noexcept
    {
    }

    [[nodiscard]] T* allocate(std::size_t n) { return RNDR_NEW_GAME_FRAME(T, n); }

    void deallocate(T* p, std::size_t n) { RNDR_DELETE_GAME_FRAME(T, p, n); }
};

template <typename T, int BlockCountPerSection = 128>
struct StdBlockAllocator
{
    using value_type = T;

    StdBlockAllocator() { m_Allocator = new BlockAllocator(sizeof(T), BlockCountPerSection); }

    template <class U>
    constexpr StdBlockAllocator(const StdBlockAllocator<U>&) noexcept
    {
        m_Allocator = new BlockAllocator(sizeof(T), BlockCountPerSection);
    }

    ~StdBlockAllocator() { delete m_Allocator; }

    [[nodiscard]] T* allocate(std::size_t n)
    {
        assert(n == 1);
        RNDR_NEW(m_Allocator, T, n);
    }

    void deallocate(T* p, std::size_t n)
    {
        assert(n == 1);
        RNDR_DELETE(m_Allocator, T, p, n);
    }

private:
    Allocator* m_Allocator;
};

class MemoryManager
{
public:
    static MemoryManager* Get();

    void Init();
    void ShutDown();

    void AddFrameAllocator(std::thread::id ThreadId, uint64_t Capacity);

    Allocator* GetPersistentAllocator();

    Allocator* GetFrameAllocator(std::thread::id ThreadId);
    Allocator* GetFrameAllocatorSafe(std::thread::id ThreadId);

    Allocator* GetGameFrameAllocator();
    Allocator* GetGameFrameAllocatorSafe();

public:
    using AllocatorUP = std::unique_ptr<Allocator, StdPersistentDeleter<Allocator>>;
    using AllocatorMap =
        std::map<std::thread::id,
                 Allocator*,
                 std::less<std::thread::id>,
                 StdPersistentAllocator<std::pair<const std::thread::id, Allocator*>>>;

private:
    static MemoryManager s_Manager;

    std::unique_ptr<Allocator> m_PersistentAllocator;

    std::mutex m_FrameAllocatorsGuard;
    AllocatorMap* m_FrameAllocators;

    std::thread::id m_GameThreadId;
};

}  // namespace rndr