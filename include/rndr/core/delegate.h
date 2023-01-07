#pragma once

#include <functional>
#include <map>

using DelegateHandle = int;

static constexpr DelegateHandle kInvalidDelegateHandle = -1;

template <typename... Args>
struct Delegate
{
    using Function = std::function<void(Args...)>;

    void Set(Function Functor) { m_Functor = Functor; }

    void Execute(Args&&... Arguments)
    {
        if (m_Functor)
        {
            m_Functor(std::forward<Args>(Arguments)...);
        }
    }

private:
    Function m_Functor;
};

template <typename... Args>
struct MultiDelegate
{
    using Function = std::function<void(Args...)>;

    DelegateHandle Add(Function Functor)
    {
        DelegateHandle Handle = m_HandleGenerator++;
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

    template <typename... Args2>
    void Execute(Args2&&... Arguments)
    {
        for (auto& Pair : m_Functors)
        {
            Pair.second(std::forward<Args2>(Arguments)...);
        }
    }

private:
    std::map<DelegateHandle, Function> m_Functors;
    DelegateHandle m_HandleGenerator = 0;
};
