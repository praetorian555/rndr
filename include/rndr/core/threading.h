#include "rndr/core/rndr.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "rndr/core/math.h"

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
    static Scheduler* s_Scheduler;

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
                for (int i = Start; i < End; i++)
                {
                    F(i);
                }
            });
        Scheduler::Get()->ExecAsync(Task);
        Tasks.push_back(Task);
    }

    for (auto& Task : Tasks)
    {
        Task->WaitUntilDone();
    }
}

}  // namespace rndr