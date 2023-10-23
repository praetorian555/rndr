#include <queue>
#include <utility>
#include <variant>

#include "rndr/core/array.h"
#include "rndr/core/input.h"
#include "rndr/core/ref.h"
#include "rndr/core/stack-array.h"

namespace Rndr
{
struct ActionData
{
    Rndr::InputAction action;
    Rndr::InputCallback callback;
    Rndr::NativeWindowHandle native_window;
    Rndr::Array<Rndr::InputBinding> bindings;
};

struct InputContextData
{
    Rndr::Ref<Rndr::InputContext> context;
    Rndr::Array<ActionData> actions;

    InputContextData(InputContext& ctx) : context(ctx) {}
    ~InputContextData() = default;
};

struct ButtonData
{
    Rndr::InputPrimitive primitive;
    Rndr::InputTrigger trigger;
};

struct MousePositionData
{
    Point2f position;
    Vector2f screen_size;
};

struct RelativeMousePositionData
{
    Vector2f delta_position;
    Vector2f screen_size;
};

struct MouseWheelData
{
    int32_t delta_wheel;
};

using EventData =
    std::variant<ButtonData, MousePositionData, RelativeMousePositionData, MouseWheelData>;

struct Event
{
    Rndr::NativeWindowHandle native_window;
    EventData data;
};

using EventQueue = std::queue<Event>;

struct InputSystemData
{
    Rndr::InputContext default_context = Rndr::InputContext("Default");
    Rndr::Array<Ref<InputContextData>> contexts;
    EventQueue events;

    ~InputSystemData() = default;
};
}  // namespace Rndr

// InputAction /////////////////////////////////////////////////////////////////////////////////////

Rndr::InputAction::InputAction(String name) : m_name(std::move(name)) {}

const Rndr::String& Rndr::InputAction::GetName() const
{
    return m_name;
}

bool Rndr::InputAction::IsValid() const
{
    return !m_name.empty();
}

// InputBinding ////////////////////////////////////////////////////////////////////////////////

bool Rndr::InputBinding::operator==(const Rndr::InputBinding& other) const
{
    return primitive == other.primitive && trigger == other.trigger;
}

// InputContext ////////////////////////////////////////////////////////////////////////////////////

Rndr::InputContext::InputContext()
{
    m_context_data = RNDR_MAKE_SCOPED(InputContextData, *this);
}

Rndr::InputContext::InputContext(String name) : m_name(std::move(name))
{
    m_context_data = RNDR_MAKE_SCOPED(InputContextData, *this);
}

Rndr::InputContext::~InputContext() = default;

const Rndr::String& Rndr::InputContext::GetName() const
{
    return m_name;
}

bool Rndr::InputContext::AddAction(const Rndr::InputAction& action,
                                   const Rndr::InputActionData& data)
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
    for (const Rndr::InputBinding& binding : data.bindings)
    {
        if (!InputSystem::IsBindingValid(binding))
        {
            RNDR_LOG_ERROR("Invalid binding");
            return false;
        }
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

bool Rndr::InputContext::AddBindingToAction(const Rndr::InputAction& action,
                                            const Rndr::InputBinding& binding)
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
    if (!InputSystem::IsBindingValid(binding))
    {
        RNDR_LOG_ERROR("Invalid binding");
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

bool Rndr::InputContext::RemoveBindingFromAction(const Rndr::InputAction& action,
                                                 const Rndr::InputBinding& binding)
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

bool Rndr::InputContext::ContainsAction(const Rndr::InputAction& action) const
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

Rndr::InputCallback Rndr::InputContext::GetActionCallback(const Rndr::InputAction& action) const
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

Rndr::Span<Rndr::InputBinding> Rndr::InputContext::GetActionBindings(
    const Rndr::InputAction& action) const
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
inline Rndr::InputSystem& GetInputSystem()
{
    static Rndr::InputSystem s_input_system;
    return s_input_system;
}
}  // namespace

Rndr::ScopePtr<Rndr::InputSystemData> Rndr::InputSystem::g_system_data;

bool Rndr::InputSystem::Init()
{
    if (g_system_data)
    {
        return true;
    }
    g_system_data = RNDR_MAKE_SCOPED(InputSystemData);
    const Ref<InputContextData> default_context_data =
        Ref{g_system_data->default_context.m_context_data.get()};
    g_system_data->contexts.push_back(default_context_data);
    return true;
}

bool Rndr::InputSystem::Destroy()
{
    if (!g_system_data)
    {
        return true;
    }
    g_system_data.reset();
    return true;
}

Rndr::InputContext& Rndr::InputSystem::GetCurrentContext()
{
    RNDR_ASSERT(g_system_data != nullptr);
    return g_system_data->contexts.back()->context.Get();
}

bool Rndr::InputSystem::PushContext(const Rndr::InputContext& context)
{
    if (g_system_data == nullptr)
    {
        return false;
    }
    g_system_data->contexts.emplace_back(*context.m_context_data.get());
    return true;
}

bool Rndr::InputSystem::PopContext()
{
    if (g_system_data == nullptr)
    {
        return false;
    }
    if (g_system_data->contexts.size() == 1)
    {
        RNDR_LOG_ERROR("Cannot pop default context");
        return false;
    }
    g_system_data->contexts.pop_back();
    return true;
}

bool Rndr::InputSystem::SubmitButtonEvent(NativeWindowHandle window,
                                          InputPrimitive primitive,
                                          InputTrigger trigger)
{
    if (g_system_data == nullptr)
    {
        return false;
    }
    const Event event{window, ButtonData{primitive, trigger}};
    g_system_data->events.push(event);
    return true;
}

bool Rndr::InputSystem::SubmitMousePositionEvent(NativeWindowHandle window,
                                                 const Point2f& position,
                                                 const Vector2f& screen_size)
{
    if (g_system_data == nullptr)
    {
        return false;
    }
    if (screen_size.x == 0 || screen_size.y == 0)
    {
        return false;
    }
    const Event event{window, MousePositionData{position, screen_size}};
    g_system_data->events.push(event);
    return true;
}

bool Rndr::InputSystem::SubmitRelativeMousePositionEvent(NativeWindowHandle window,
                                                         const Vector2f& delta_position,
                                                         const Vector2f& screen_size)
{
    if (g_system_data == nullptr)
    {
        return false;
    }
    if (screen_size.x == 0 || screen_size.y == 0)
    {
        return false;
    }
    const Event event{window, RelativeMousePositionData{delta_position, screen_size}};
    g_system_data->events.push(event);
    return true;
}

bool Rndr::InputSystem::SubmitMouseWheelEvent(NativeWindowHandle window, int32_t delta_wheel)
{
    if (g_system_data == nullptr)
    {
        return false;
    }
    const Event event{window, MouseWheelData{delta_wheel}};
    g_system_data->events.push(event);
    return true;
}

namespace Rndr
{
struct InputEventProcessor
{
    Ref<const InputContextData> context;
    NativeWindowHandle native_window;

    void operator()(const ButtonData& event) const;
    void operator()(const MousePositionData& event) const;
    void operator()(const RelativeMousePositionData& event) const;
    void operator()(const MouseWheelData& event) const;
};

}  // namespace Rndr

bool Rndr::InputSystem::ProcessEvents(float delta_seconds)
{
    RNDR_UNUSED(delta_seconds);

    if (g_system_data == nullptr)
    {
        return false;
    }
    EventQueue& events = g_system_data->events;
    const InputContextData& context = g_system_data->contexts.back();
    while (!events.empty())
    {
        Event event = events.front();
        events.pop();
        std::visit(InputEventProcessor{Ref(context), event.native_window}, event.data);
    }
    return true;
}

void Rndr::InputEventProcessor::operator()(const ButtonData& event) const
{
    for (const ActionData& action_data : context->actions)
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

void Rndr::InputEventProcessor::operator()(const MousePositionData& event) const
{
    const StackArray<InputPrimitive, 2> axes = {InputPrimitive::Mouse_AxisX,
                                                InputPrimitive::Mouse_AxisY};
    const InputTrigger trigger = InputTrigger::AxisChangedAbsolute;
    for (const ActionData& action_data : context->actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == axes[0] && binding.trigger == trigger)
            {
                const float value = binding.modifier * event.position[0];
                action_data.callback(axes[0], trigger, value);
            }
            if (binding.primitive == axes[1] && binding.trigger == trigger)
            {
                const float value = binding.modifier * event.position[1];
                action_data.callback(axes[1], trigger, value);
            }
        }
    }
}

void Rndr::InputEventProcessor::operator()(const RelativeMousePositionData& event) const
{
    const StackArray<InputPrimitive, 2> axes = {InputPrimitive::Mouse_AxisX,
                                                InputPrimitive::Mouse_AxisY};
    const InputTrigger trigger = InputTrigger::AxisChangedRelative;
    for (const ActionData& action_data : context->actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == axes[0] && binding.trigger == trigger)
            {
                const float value = binding.modifier * event.delta_position[0];
                action_data.callback(axes[0], trigger, value);
            }
            if (binding.primitive == axes[1] && binding.trigger == trigger)
            {
                const float value = binding.modifier * event.delta_position[1];
                action_data.callback(axes[1], trigger, value);
            }
        }
    }
}

void Rndr::InputEventProcessor::operator()(const MouseWheelData& event) const
{
    const InputPrimitive primitive = InputPrimitive::Mouse_AxisWheel;
    const InputTrigger trigger = InputTrigger::AxisChangedRelative;
    for (const ActionData& action_data : context->actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == primitive && binding.trigger == trigger)
            {
                const float value = binding.modifier * static_cast<float>(event.delta_wheel);
                action_data.callback(primitive, trigger, value);
                break;
            }
        }
    }
}

bool Rndr::InputSystem::IsButton(InputPrimitive primitive)
{
    return IsKeyboardButton(primitive) || IsMouseButton(primitive);
}

bool Rndr::InputSystem::IsMouseButton(InputPrimitive primitive)
{
    using enum Rndr::InputPrimitive;
    return primitive == Mouse_LeftButton || primitive == Mouse_RightButton
           || primitive == Mouse_MiddleButton;
}

bool Rndr::InputSystem::IsKeyboardButton(InputPrimitive primitive)

{
    return primitive > InputPrimitive::_KeyboardStart && primitive < InputPrimitive::_KeyboardEnd;
}

bool Rndr::InputSystem::IsAxis(InputPrimitive primitive)
{
    return primitive == InputPrimitive::Mouse_AxisX || primitive == InputPrimitive::Mouse_AxisY
           || IsMouseWheelAxis(primitive);
}

bool Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive primitive)
{
    return primitive == InputPrimitive::Mouse_AxisWheel;
}

bool Rndr::InputSystem::IsBindingValid(const Rndr::InputBinding& binding)
{
    using enum Rndr::InputTrigger;
    if (IsButton(binding.primitive))
    {
        return binding.trigger == ButtonPressed || binding.trigger == ButtonReleased
               || binding.trigger == ButtonDoubleClick;
    }
    if (IsMouseWheelAxis(binding.primitive))
    {
        return binding.trigger == AxisChangedRelative;
    }
    if (IsAxis(binding.primitive))
    {
        return binding.trigger == AxisChangedAbsolute || binding.trigger == AxisChangedRelative;
    }
    return false;
}
