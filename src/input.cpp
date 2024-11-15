#include <queue>
#include <utility>
#include <variant>

#include "opal/container/dynamic-array.h"
#include "opal/container/ref.h"
#include "opal/container/in-place-array.h"

#include "rndr/input.h"
#include "rndr/log.h"
#include "rndr/types.h"

namespace Rndr
{
struct ActionData
{
    InputAction action;
    InputCallback callback;
    NativeWindowHandle native_window;
    Opal::DynamicArray<InputBinding> bindings;
};

struct InputContextData
{
    Opal::Ref<InputContext> context;
    Opal::DynamicArray<ActionData> actions;

    explicit InputContextData(InputContext& ctx) : context(ctx) {}
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
    i32 delta_wheel;
};

using EventData = std::variant<ButtonData, MousePositionData, RelativeMousePositionData, MouseWheelData>;

struct Event
{
    Rndr::NativeWindowHandle native_window;
    EventData data;
};

using EventQueue = std::queue<Event>;

struct InputSystemData
{
    InputContext default_context = InputContext(Opal::StringUtf8("Default"));
    Opal::DynamicArray<Opal::Ref<InputContextData>> contexts;
    EventQueue events;

    ~InputSystemData() = default;
};
}  // namespace Rndr

// InputAction /////////////////////////////////////////////////////////////////////////////////////

Rndr::InputAction::InputAction(Opal::StringUtf8 name) : m_name(std::move(name)) {}

const Opal::StringUtf8& Rndr::InputAction::GetName() const
{
    return m_name;
}

bool Rndr::InputAction::IsValid() const
{
    return !m_name.IsEmpty();
}

// InputBinding ////////////////////////////////////////////////////////////////////////////////

bool Rndr::InputBinding::operator==(const Rndr::InputBinding& other) const
{
    return primitive == other.primitive && trigger == other.trigger;
}

// InputContext ////////////////////////////////////////////////////////////////////////////////////

Rndr::InputContext::InputContext()
{
    m_context_data = Opal::MakeDefaultScoped<InputContextData>(*this);
}

Rndr::InputContext::InputContext(Opal::StringUtf8 name) : m_name(std::move(name))
{
    m_context_data = Opal::MakeDefaultScoped<InputContextData>(*this);
}

Rndr::InputContext::~InputContext() = default;

const Opal::StringUtf8& Rndr::InputContext::GetName() const
{
    return m_name;
}

bool Rndr::InputContext::AddAction(const Rndr::InputAction& action, const Rndr::InputActionData& data)
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
    action_data.bindings.Assign(data.bindings.begin(), data.bindings.end());
    m_context_data->actions.PushBack(action_data);
    return true;
}

bool Rndr::InputContext::AddBindingToAction(const Rndr::InputAction& action, const Rndr::InputBinding& binding)
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
        action_data.bindings.PushBack(binding);
        return true;
    }
    RNDR_LOG_ERROR("Action does not exist");
    return false;
}

bool Rndr::InputContext::RemoveBindingFromAction(const Rndr::InputAction& action, const Rndr::InputBinding& binding)
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
            for (auto it = action_data.bindings.cbegin(); it != action_data.bindings.cend(); ++it)
            {
                if (*it != binding)
                {
                    continue;
                }
                action_data.bindings.Erase(it);
                return true;
            }
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
    for (const ActionData& action_data : m_context_data->actions)
    {
        if (action_data.action == action)
        {
            return true;
        }
    }
    return false;
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
    for (const ActionData& action_data : m_context_data->actions)
    {
        if (action_data.action == action)
        {
            return action_data.callback;
        }
    }
    return nullptr;
}

Opal::ArrayView<Rndr::InputBinding> Rndr::InputContext::GetActionBindings(const Rndr::InputAction& action) const
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
    for (ActionData& action_data : m_context_data->actions)
    {
        if (action_data.action == action)
        {
            return Opal::ArrayView<InputBinding>{action_data.bindings.begin(), action_data.bindings.end()};
        }
    }
    return Opal::ArrayView<InputBinding>{};
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

Opal::ScopePtr<Rndr::InputSystemData> Rndr::InputSystem::g_system_data;

bool Rndr::InputSystem::Init()
{
    if (!g_system_data)
    {
        g_system_data = Opal::MakeDefaultScoped<InputSystemData>();
    }
    const Opal::Ref<InputContextData> default_context_data = Opal::Ref{g_system_data->default_context.m_context_data.Get()};
    g_system_data->contexts.PushBack(default_context_data);
    return true;
}

bool Rndr::InputSystem::Destroy()
{
    if (!g_system_data)
    {
        return true;
    }
    g_system_data.Reset(nullptr);
    return true;
}

Rndr::InputContext& Rndr::InputSystem::GetCurrentContext()
{
    RNDR_ASSERT(g_system_data != nullptr);
    return g_system_data->contexts.Back().GetValue()->context.Get();
}

bool Rndr::InputSystem::PushContext(const Rndr::InputContext& context)
{
    if (g_system_data == nullptr)
    {
        return false;
    }
    g_system_data->contexts.PushBack(Opal::Ref(*context.m_context_data.Get()));
    return true;
}

bool Rndr::InputSystem::PopContext()
{
    if (g_system_data == nullptr)
    {
        return false;
    }
    if (g_system_data->contexts.GetSize() == 1)
    {
        RNDR_LOG_ERROR("Cannot pop default context");
        return false;
    }
    g_system_data->contexts.PopBack();
    return true;
}

bool Rndr::InputSystem::SubmitButtonEvent(NativeWindowHandle window, InputPrimitive primitive, InputTrigger trigger)
{
    if (g_system_data == nullptr)
    {
        return false;
    }
    const Event event{window, ButtonData{primitive, trigger}};
    g_system_data->events.push(event);
    return true;
}

bool Rndr::InputSystem::SubmitMousePositionEvent(NativeWindowHandle window, const Point2f& position, const Vector2f& screen_size)
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

bool Rndr::InputSystem::SubmitRelativeMousePositionEvent(NativeWindowHandle window, const Vector2f& delta_position,
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

bool Rndr::InputSystem::SubmitMouseWheelEvent(NativeWindowHandle window, i32 delta_wheel)
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
    Opal::Ref<const InputContextData> context;
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
    const InputContextData& context = g_system_data->contexts.Back().GetValue();
    while (!events.empty())
    {
        Event event = events.front();
        events.pop();
        std::visit(InputEventProcessor{Opal::Ref(context), event.native_window}, event.data);
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
    const Opal::InPlaceArray<InputPrimitive, 2> axes = {InputPrimitive::Mouse_AxisX, InputPrimitive::Mouse_AxisY};
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
    const Opal::InPlaceArray<InputPrimitive, 2> axes = {InputPrimitive::Mouse_AxisX, InputPrimitive::Mouse_AxisY};
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
    return primitive == Mouse_LeftButton || primitive == Mouse_RightButton || primitive == Mouse_MiddleButton;
}

bool Rndr::InputSystem::IsKeyboardButton(InputPrimitive primitive)

{
    return primitive > InputPrimitive::_KeyboardStart && primitive < InputPrimitive::_KeyboardEnd;
}

bool Rndr::InputSystem::IsAxis(InputPrimitive primitive)
{
    return primitive == InputPrimitive::Mouse_AxisX || primitive == InputPrimitive::Mouse_AxisY || IsMouseWheelAxis(primitive);
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
        return binding.trigger == ButtonPressed || binding.trigger == ButtonReleased || binding.trigger == ButtonDoubleClick;
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
