#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "rndr/core/math.h"

#include "rndr/profiling/cputracer.h"

#define RNDR_ENABLE_MULTITHREADING (0)

namespace rndr
{

/**
 * Base interface for tasks.
 */
struct TaskBase
{
    virtual ~TaskBase() = default;
    virtual void Execute() = 0;
    virtual bool WaitUntilDone() = 0;
};

/**
 * Shared pointer to the TaskBase.
 */
using TaskBaseSP = std::shared_ptr<TaskBase>;

/**
 * Task that executes a function with no arguments and no return values.
 */
template <typename Function>
class Task : public TaskBase
{
public:
    Task(Function F) : m_Function(F) {}

    virtual void Execute() override
    {
        m_Function();
        m_Done = true;
    }

    virtual bool WaitUntilDone() override
    {
        while (!m_Done)
            continue;

        return true;
    }

private:
    Function m_Function;
    bool m_Done = false;
};

/**
 * Basic thread-safe queue that uses std::queue.
 */
template <typename T>
class ThreadSafeQueue
{
public:
    void Push(const T& Value)
    {
        auto Lock = std::unique_lock(m_Guard);
        m_Queue.push(Value);
        m_Signal.notify_one();
    }

    T Pop()
    {
        auto Lock = std::unique_lock(m_Guard);
        m_Signal.wait(Lock, [this] { return !m_Queue.empty(); });
        T Value = m_Queue.front();
        m_Queue.pop();
        return Value;
    }

private:
    std::queue<T> m_Queue;
    std::mutex m_Guard;
    std::condition_variable m_Signal;
};

/**
 * Singleton class used to schedule async work using tasks.
 */
class Scheduler
{
public:
    static Scheduler* Get();

    void Init();
    void ShutDown();

    void ExecAsync(const TaskBaseSP& Task);

protected:
    Scheduler() = default;

private:
    static std::unique_ptr<Scheduler> s_Scheduler;

    ThreadSafeQueue<TaskBaseSP> m_Queue;
    std::vector<std::thread> m_Threads;
};

/**
 * Helper function to spawn a Task that executes a function with no arguments and stores it in a
 * shared pointer to base interface.
 *
 * @param F Function without arguments and no return value to execute.
 * @return Returns a shared pointer to the base interface TaskBase.
 */
template <typename Function>
TaskBaseSP MakeTask(Function F);

/**
 * Runs an iteration from 0 to End index and executing function at each step.
 *
 * @param End Last index of the iteration. It is not executed.
 * @param BatchSize How many iterations to run on one thread.
 * @param F Function to run at each iteration. Function needs to take an int argument and it mustn't
 * return any value.
 */
template <typename Function>
void ParallelFor(int End, int BatchSize, Function F);

template <typename Function>
void ParallelFor(const Point2i End, int BatchSize, Function F);

template <typename Function>
void ParallelFor(const Vector2i End, int BatchSize, Function F);

// Implementation /////////////////////////////////////////////////////////////////////////////////

template <typename Function>
TaskBaseSP MakeTask(Function F)
{
    return TaskBaseSP{new Task(F)};
}

template <typename Function>
void ParallelFor(int End, int BatchSize, Function F)
{
    std::vector<TaskBaseSP> Tasks;
    for (int i = 0; i < End; i += BatchSize)
    {
        TaskBaseSP Task = MakeTask(
            [Start = i, End = std::min(i + BatchSize, End), F]
            {
                RNDR_CPU_TRACE("Parallel For");

                for (int i = Start; i < End; i++)
                {
                    F(i);
                }
            });
#if RNDR_ENABLE_MULTITHREADING
        Scheduler::Get()->ExecAsync(Task);
#endif  // RNDR_ENABLE_MULTITHREADING
        Tasks.push_back(Task);
    }

    for (auto& Task : Tasks)
    {
#if RNDR_ENABLE_MULTITHREADING
        Task->WaitUntilDone();
#else
        Task->Execute();
#endif  // RNDR_ENABLE_MULTITHREADING
    }
}

template <typename Function>
void ParallelFor(const Point2i End, int BatchSize, Function F)
{
    std::vector<TaskBaseSP> Tasks;
    for (int Y = 0; Y < End.Y; Y += BatchSize)
    {
        for (int X = 0; X < End.X; X += BatchSize)
        {
            TaskBaseSP Task = MakeTask(
                [StartX = X, StartY = Y, EndX = std::min(X + BatchSize, End.X),
                 EndY = std::min(Y + BatchSize, End.Y), F]
                {
                    RNDR_CPU_TRACE("Parallel For 2D");

                    for (int Y = StartY; Y < EndY; Y++)
                    {
                        for (int X = StartX; X < EndX; X++)
                        {
                            F(X, Y);
                        }
                    }
                });
#if RNDR_ENABLE_MULTITHREADING
            Scheduler::Get()->ExecAsync(Task);
#endif  // RNDR_ENABLE_MULTITHREADING
            Tasks.push_back(Task);
        }
    }

    for (auto& Task : Tasks)
    {
#if RNDR_ENABLE_MULTITHREADING
        Task->WaitUntilDone();
#else
        Task->Execute();
#endif  // RNDR_ENABLE_MULTITHREADING
    }
}

template <typename Function>
void ParallelFor(const Vector2i End, int BatchSize, Function F)
{
    ParallelFor(Point2i{End.X, End.Y}, BatchSize, F);
}

}  // namespace rndr