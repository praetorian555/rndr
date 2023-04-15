#pragma once

#include <functional>
// TODO(Marko): Remove this once we have custom containers
#include <map>

namespace Rndr
{

using DelegateHandle = int;
static constexpr DelegateHandle k_invalid_delegate_handle = -1;

template <typename ReturnType, typename... Args>
struct Delegate;

template <typename ReturnType, typename... Args>
struct Delegate<ReturnType(Args...)>
{
    using Function = std::function<ReturnType(Args...)>;

    void Bind(Function functor) { m_functor = functor; }
    void Unbind() { m_functor = nullptr; }

    [[nodiscard]] bool IsBound() const { return m_functor != nullptr; }

    template <typename... ExecArgs>
    ReturnType Execute(ExecArgs&&... arguments)
    {
        if (m_functor)
        {
            return m_functor(std::forward<ExecArgs>(arguments)...);
        }

        return ReturnType{};
    }

private:
    Function m_functor;
};

template <typename... Args>
struct MultiDelegate;

template <typename... Args>
struct MultiDelegate<void(Args...)>
{
    using Function = std::function<void(Args...)>;

    DelegateHandle Bind(Function functor)
    {
        const DelegateHandle handle = m_handle_generator++;
        m_functors.insert(std::make_pair(handle, functor));
        return handle;
    }

    void Unbind(DelegateHandle handle)
    {
        if (handle == k_invalid_delegate_handle)
        {
            return;
        }

        auto iterator = m_functors.find(handle);
        if (iterator != m_functors.end())
        {
            m_functors.erase(iterator);
        }
    }

    [[nodiscard]] bool IsBound(DelegateHandle handle) const
    {
        return m_functors.find(handle) != m_functors.end();
    }

    [[nodiscard]] bool IsAnyBound() const
    {
        return !m_functors.empty();
    }

    template <typename... ExecArgs>
    void Execute(ExecArgs&&... arguments)
    {
        for (auto& pair : m_functors)
        {
            pair.second(std::forward<ExecArgs>(arguments)...);
        }
    }

private:
    std::map<DelegateHandle, Function> m_functors;
    DelegateHandle m_handle_generator = 0;
};

}  // namespace Rndr
