#pragma once

#include <functional>
#include <map>

using DelegateHandle = int;

static constexpr DelegateHandle kInvalidDelegateHandle = -1;

template <typename ReturnType, typename... Args>
struct Delegate;

template <typename ReturnType, typename... Args>
struct Delegate<ReturnType(Args...)>
{
    using Function = std::function<ReturnType(Args...)>;

    void Set(Function Functor) { m_Functor = Functor; }

    template <typename... ExecArgs>
    ReturnType Execute(ExecArgs&&... Arguments)
    {
        if (m_Functor)
        {
            return m_Functor(std::forward<ExecArgs>(Arguments)...);
        }

        return ReturnType{};
    }

private:
    Function m_Functor;
};

// TODO(Marko): Create partial specialization like for the Delegate template
template <typename... Args>
struct MultiDelegate
{
    using Function = std::function<void(Args...)>;

    DelegateHandle Add(Function Functor)
    {
        const DelegateHandle Handle = m_HandleGenerator++;
        m_Functors.insert(std::make_pair(Handle, Functor));
        return Handle;
    }

    void Remove(DelegateHandle Handle)
    {
        if (Handle == kInvalidDelegateHandle)
        {
            return;
        }

        auto Iterator = m_Functors.find(Handle);
        if (Iterator != m_Functors.end())
        {
            m_Functors.erase(Iterator);
        }
    }

    template <typename... ExecArgs>
    void Execute(ExecArgs&&... Arguments)
    {
        for (auto& Pair : m_Functors)
        {
            Pair.second(std::forward<ExecArgs>(Arguments)...);
        }
    }

private:
    std::map<DelegateHandle, Function> m_Functors;
    DelegateHandle m_HandleGenerator = 0;
};
