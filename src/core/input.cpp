#include <queue>
#include <utility>
#include <variant>

#include "rndr/core/array.h"
#include "rndr/core/input.h"
#include "rndr/core/ref.h"
#include "rndr/core/stack-array.h"

namespace rndr
{
struct ActionData
{
    rndr::InputAction action;
    rndr::InputCallback callback;
    rndr::OpaquePtr native_window;
    rndr::Array<rndr::InputBinding> bindings;
};

struct InputContextData
{
    rndr::Array<ActionData> actions;
    rndr::Ref<rndr::InputContext> context;

    ~InputContextData() = default;
};

struct ButtonData
{
    rndr::InputPrimitive primitive;
    rndr::InputTrigger trigger;
};

struct MousePositionData
{
    math::Point2 position;
    math::Vector2 screen_size;
};

struct RelativeMousePositionData
{
    math::Vector2 delta_position;
    math::Vector2 screen_size;
};

struct MouseWheelData
{
    int32_t delta_wheel;
};

using EventData =
    std::variant<ButtonData, MousePositionData, RelativeMousePositionData, MouseWheelData>;

struct Event
{
    rndr::OpaquePtr native_window;
    EventData data;
};

using EventQueue = std::queue<Event>;

struct InputSystemData
{
    rndr::InputContext default_context = rndr::InputContext("Default");
    rndr::Array<Ref<InputContextData>> contexts;
    EventQueue events;

    ~InputSystemData() = default;
};
}  // namespace rndr

// InputAction /////////////////////////////////////////////////////////////////////////////////////

rndr::InputAction::InputAction(String name) : m_name(std::move(name)) {}

const rndr::String& rndr::InputAction::GetName() const
{
    return m_name;
}

bool rndr::InputAction::IsValid() const
{
    return !m_name.empty();
}

// InputBinding ////////////////////////////////////////////////////////////////////////////////

bool rndr::InputBinding::operator==(const rndr::InputBinding& other) const
{
    return primitive == other.primitive && trigger == other.trigger;
}

// InputContext ////////////////////////////////////////////////////////////////////////////////////

rndr::InputContext::InputContext()
{
    m_context_data = RNDR_MAKE_SCOPED(InputContextData, {}, *this);
}

rndr::InputContext::InputContext(String name) : m_name(std::move(name))
{
    m_context_data = RNDR_MAKE_SCOPED(InputContextData, {}, *this);
}

rndr::InputContext::~InputContext() = default;

const rndr::String& rndr::InputContext::GetName() const
{
    return m_name;
}

bool rndr::InputContext::AddAction(const rndr::InputAction& action,
                                   const rndr::InputActionData& data)
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return false;
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return false;
    }
    if (data.callback == nullptr)
    {
        RNDR_LOG_ERROR("Invalid callback");
        return false;
    }
    for (const ActionData& action_data : m_context_data->actions)
    {
        if (action_data.action == action)
        {
            RNDR_LOG_ERROR("Action already exists");
            return false;
        }
    }
    ActionData action_data;
    action_data.action = action;
    action_data.callback = data.callback;
    action_data.native_window = data.native_window;
    action_data.bindings.assign(data.bindings.begin(), data.bindings.end());
    m_context_data->actions.push_back(action_data);
    return true;
}

bool rndr::InputContext::AddBindingToAction(const rndr::InputAction& action,
                                            const rndr::InputBinding& binding)
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return false;
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return false;
    }

    for (ActionData& action_data : m_context_data->actions)
    {
        if (action_data.action != action)
        {
            continue;
        }
        for (InputBinding& existing_binding : action_data.bindings)
        {
            if (existing_binding != binding)
            {
                continue;
            }
            existing_binding = binding;
            return true;
        }
        action_data.bindings.push_back(binding);
        return true;
    }
    RNDR_LOG_ERROR("Action does not exist");
    return false;
}

bool rndr::InputContext::RemoveBindingFromAction(const rndr::InputAction& action,
                                                 const rndr::InputBinding& binding)
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return false;
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return false;
    }

    for (ActionData& action_data : m_context_data->actions)
    {
        if (action_data.action == action)
        {
            std::erase_if(action_data.bindings,
                          [&binding](const InputBinding& existing_binding)
                          { return existing_binding == binding; });
            return true;
        }
    }
    RNDR_LOG_ERROR("Action does not exist");
    return false;
}

bool rndr::InputContext::ContainsAction(const rndr::InputAction& action) const
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return false;
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return false;
    }
    return std::ranges::any_of(m_context_data->actions,
                               [&action](const auto& action_data)
                               { return action_data.action == action; });
}

rndr::InputCallback rndr::InputContext::GetActionCallback(const rndr::InputAction& action) const
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return nullptr;
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return nullptr;
    }
    const auto& action_iter = std::ranges::find_if(m_context_data->actions,
                                                   [&action](const auto& action_data)
                                                   { return action_data.action == action; });
    return action_iter != m_context_data->actions.end() ? action_iter->callback : nullptr;
}

rndr::Span<rndr::InputBinding> rndr::InputContext::GetActionBindings(
    const rndr::InputAction& action) const
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return {};
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return {};
    }
    const auto& action_iter = std::ranges::find_if(m_context_data->actions,
                                                   [&action](const auto& action_data)
                                                   { return action_data.action == action; });
    return action_iter != m_context_data->actions.end() ? action_iter->bindings
                                                        : Span<InputBinding>{};
}

// InputSystem ////////////////////////////////////////////////////////////////////////////////////

namespace
{
inline rndr::InputSystem& GetInputSystem()
{
    static rndr::InputSystem s_input_system;
    return s_input_system;
}
}  // namespace

rndr::ScopePtr<rndr::InputSystemData> rndr::InputSystem::g_system_data;

bool rndr::InputSystem::Init()
{
    if (g_system_data)
    {
        return true;
    }
    g_system_data = RNDR_MAKE_SCOPED(InputSystemData);
    const Ref<InputContextData> default_context_data =
        *g_system_data->default_context.m_context_data.get();
    g_system_data->contexts.push_back(default_context_data);
    return true;
}

bool rndr::InputSystem::Destroy()
{
    if (!g_system_data)
    {
        return true;
    }
    g_system_data.reset();
    return true;
}

rndr::InputContext& rndr::InputSystem::GetCurrentContext()
{
    assert(g_system_data != nullptr);
    return g_system_data->contexts.back().get().context;
}

bool rndr::InputSystem::PushContext(const rndr::InputContext& context)
{
    assert(g_system_data != nullptr);
    g_system_data->contexts.emplace_back(*context.m_context_data.get());
    return true;
}

bool rndr::InputSystem::PopContext()
{
    assert(g_system_data != nullptr);
    if (g_system_data->contexts.size() == 1)
    {
        RNDR_LOG_ERROR("Cannot pop default context");
        return false;
    }
    g_system_data->contexts.pop_back();
    return true;
}

bool rndr::InputSystem::SubmitButtonEvent(OpaquePtr window,
                                          InputPrimitive primitive,
                                          InputTrigger trigger)
{
    assert(g_system_data != nullptr);
    const Event event{window, ButtonData{primitive, trigger}};
    g_system_data->events.push(event);
    return true;
}

bool rndr::InputSystem::SubmitMousePositionEvent(OpaquePtr window,
                                                 const math::Point2& position,
                                                 const math::Vector2& screen_size)
{
    assert(g_system_data != nullptr);
    if (screen_size.X == 0 || screen_size.Y == 0)
    {
        return false;
    }
    const Event event{window, MousePositionData{position, screen_size}};
    g_system_data->events.push(event);
    return true;
}

bool rndr::InputSystem::SubmitRelativeMousePositionEvent(OpaquePtr window,
                                                         const math::Vector2& delta_position,
                                                         const math::Vector2& screen_size)
{
    assert(g_system_data != nullptr);
    if (screen_size.X == 0 || screen_size.Y == 0)
    {
        return false;
    }
    const Event event{window, RelativeMousePositionData{delta_position, screen_size}};
    g_system_data->events.push(event);
    return true;
}

bool rndr::InputSystem::SubmitMouseWheelEvent(OpaquePtr window, int32_t delta_wheel)
{
    const Event event{window, MouseWheelData{delta_wheel}};
    g_system_data->events.push(event);
    return true;
}

namespace rndr
{
struct InputEventProcessor
{
    Ref<const InputContextData> context;
    OpaquePtr native_window;

    void operator()(const ButtonData& event) const;
    void operator()(const MousePositionData& event) const;
    void operator()(const RelativeMousePositionData& event) const;
    void operator()(const MouseWheelData& event) const;
};

}  // namespace rndr

bool rndr::InputSystem::ProcessEvents(real delta_seconds)
{
    RNDR_UNUSED(delta_seconds);

    assert(g_system_data != nullptr);
    EventQueue& events = g_system_data->events;
    const InputContextData& context = g_system_data->contexts.back();
    while (!events.empty())
    {
        Event event = events.front();
        events.pop();
        std::visit(InputEventProcessor{context, event.native_window}, event.data);
    }
    return true;
}

void rndr::InputEventProcessor::operator()(const ButtonData& event) const
{
    for (const ActionData& action_data : context.get().actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == event.primitive && binding.trigger == event.trigger)
            {
                action_data.callback(event.primitive, event.trigger, binding.modifier);
                break;
            }
        }
    }
}

void rndr::InputEventProcessor::operator()(const MousePositionData& event) const
{
    const StackArray<InputPrimitive, 2> axes = {InputPrimitive::Mouse_AxisX,
                                                InputPrimitive::Mouse_AxisY};
    const InputTrigger trigger = InputTrigger::AxisChangedAbsolute;
    for (const ActionData& action_data : context.get().actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == axes[0] && binding.trigger == trigger)
            {
                const real value = binding.modifier * event.position[0];
                action_data.callback(axes[0], trigger, value);
            }
            if (binding.primitive == axes[1] && binding.trigger == trigger)
            {
                const real value = binding.modifier * event.position[1];
                action_data.callback(axes[1], trigger, value);
            }
        }
    }
}

void rndr::InputEventProcessor::operator()(const RelativeMousePositionData& event) const
{
    const StackArray<InputPrimitive, 2> axes = {InputPrimitive::Mouse_AxisX,
                                                InputPrimitive::Mouse_AxisY};
    const InputTrigger trigger = InputTrigger::AxisChangedRelative;
    for (const ActionData& action_data : context.get().actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == axes[0] && binding.trigger == trigger)
            {
                const real value = binding.modifier * event.delta_position[0];
                action_data.callback(axes[0], trigger, value);
            }
            if (binding.primitive == axes[1] && binding.trigger == trigger)
            {
                const real value = binding.modifier * event.delta_position[1];
                action_data.callback(axes[1], trigger, value);
            }
        }
    }
}

void rndr::InputEventProcessor::operator()(const MouseWheelData& event) const
{
    const InputPrimitive primitive = InputPrimitive::Mouse_AxisWheel;
    const InputTrigger trigger = InputTrigger::AxisChangedRelative;
    for (const ActionData& action_data : context.get().actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == primitive && binding.trigger == trigger)
            {
                const real value = binding.modifier * static_cast<real>(event.delta_wheel);
                action_data.callback(primitive, trigger, value);
                break;
            }
        }
    }
}

bool rndr::InputSystem::IsButton(InputPrimitive primitive)
{
    return IsKeyboardButton(primitive) || IsMouseButton(primitive);
}

bool rndr::InputSystem::IsMouseButton(InputPrimitive primitive)
{
    using enum rndr::InputPrimitive;
    return primitive == Mouse_LeftButton || primitive == Mouse_RightButton
           || primitive == Mouse_MiddleButton;
}

bool rndr::InputSystem::IsKeyboardButton(InputPrimitive primitive)

{
    return primitive > InputPrimitive::_KeyboardStart && primitive < InputPrimitive::_KeyboardEnd;
}

bool rndr::InputSystem::IsAxis(InputPrimitive primitive)
{
    return primitive == InputPrimitive::Mouse_AxisX || primitive == InputPrimitive::Mouse_AxisY;
}

bool rndr::InputSystem::IsMouseWheel(rndr::InputPrimitive primitive)
{
    return primitive == InputPrimitive::Mouse_AxisWheel;
}
