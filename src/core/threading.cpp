#include "rndr/core/threading.h"

#include "rndr/core/log.h"

std::unique_ptr<rndr::Scheduler> rndr::Scheduler::s_Scheduler = nullptr;

rndr::Scheduler* rndr::Scheduler::Get()
{
    if (!s_Scheduler)
    {
        s_Scheduler.reset(new Scheduler{});
    }

    return s_Scheduler.get();
}

void rndr::Scheduler::Init()
{
    auto Body = [this]
    {
        while (true)
        {
            TaskBaseSP Task;
            Task = m_Queue.Pop();
            if (!Task)
            {
                return;
            }
            Task->Execute();
        }
    };

    const int ThreadCount = std::thread::hardware_concurrency();
    RNDR_LOG_INFO("rndr::Scheduler: Using %d threads", ThreadCount);

    m_Threads.resize(ThreadCount);
    for (auto& Thread : m_Threads)
    {
        Thread = std::thread{Body};
    }
}

void rndr::Scheduler::ShutDown()
{
    TaskBaseSP KillTask{nullptr};
    for (const auto& Thread : m_Threads)
    {
        m_Queue.Push(KillTask);
    }

    for (auto& Thread : m_Threads)
    {
        Thread.join();
    }
}

void rndr::Scheduler::ExecAsync(const TaskBaseSP& Task)
{
    m_Queue.Push(Task);
}