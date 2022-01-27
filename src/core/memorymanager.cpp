#include "rndr/core/memorymanager.h"

#include "rndr/memory/stackallocator.h"

rndr::MemoryManager rndr::MemoryManager::s_Manager;

rndr::MemoryManager* rndr::MemoryManager::Get()
{
    return &s_Manager;
}

void rndr::MemoryManager::Init()
{
    m_GameThreadId = std::this_thread::get_id();

    if (!m_PersistentAllocator)
    {
        m_PersistentAllocator = std::make_unique<StackAllocator>(RNDR_MB(100));
    }

    m_FrameAllocators = RNDR_NEW_PERSISTENT(AllocatorMap, 1);

    AddFrameAllocator(m_GameThreadId, RNDR_MB(100));
}

void rndr::MemoryManager::ShutDown() {}

void rndr::MemoryManager::AddFrameAllocator(std::thread::id ThreadId, uint64_t Capacity)
{
    std::lock_guard<std::mutex> Lock{m_FrameAllocatorsGuard};

    Allocator* ThreadFrameAllocator = RNDR_NEW_PERSISTENT(StackAllocator, 1, Capacity);
    using PairType = std::pair<const std::thread::id, Allocator*>;
    PairType Pair{ThreadId, ThreadFrameAllocator};
    m_FrameAllocators->insert(Pair);
}

rndr::Allocator* rndr::MemoryManager::GetPersistentAllocator()
{
    if (!m_PersistentAllocator)
    {
        m_PersistentAllocator = std::make_unique<StackAllocator>(RNDR_MB(100));
    }
    return m_PersistentAllocator.get();
}

rndr::Allocator* rndr::MemoryManager::GetFrameAllocator(std::thread::id ThreadId)
{
    auto& Iter = m_FrameAllocators->find(ThreadId);
    return Iter == m_FrameAllocators->end() ? nullptr : Iter->second;
}

rndr::Allocator* rndr::MemoryManager::GetFrameAllocatorSafe(std::thread::id ThreadId)
{
    std::lock_guard<std::mutex> Lock{m_FrameAllocatorsGuard};

    auto& Iter = m_FrameAllocators->find(ThreadId);
    return Iter == m_FrameAllocators->end() ? nullptr : Iter->second;
}

rndr::Allocator* rndr::MemoryManager::GetGameFrameAllocator()
{
    auto& Iter = m_FrameAllocators->find(m_GameThreadId);
    return Iter == m_FrameAllocators->end() ? nullptr : Iter->second;
}

rndr::Allocator* rndr::MemoryManager::GetGameFrameAllocatorSafe()
{
    std::lock_guard<std::mutex> Lock{m_FrameAllocatorsGuard};

    auto& Iter = m_FrameAllocators->find(m_GameThreadId);
    return Iter == m_FrameAllocators->end() ? nullptr : Iter->second;
}
