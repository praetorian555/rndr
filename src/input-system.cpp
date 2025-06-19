#include <queue>
#include <utility>
#include <variant>

#include "opal/container/dynamic-array.h"
#include "opal/container/in-place-array.h"
#include "opal/container/ref.h"

#include "rndr/input-system.hpp"
#include "rndr/log.h"
#include "rndr/types.h"

namespace Rndr
{

struct ButtonData
{
    InputPrimitive primitive;
    InputTrigger trigger;
    bool is_repeated;
};

struct MouseButtonData
{
    InputPrimitive primitive;
    InputTrigger trigger;
    Vector2i cursor_position;  // Position of the cursor relative to the screen, not the window
};

struct CharacterData
{
    uchar32 character;
};

struct MousePositionData
{
    Vector2f delta_position;
};

struct MouseWheelData
{
    f32 delta_wheel;
    Vector2i cursor_position;  // Position of the cursor relative to the screen, not the window
};

using EventData = std::variant<ButtonData, MouseButtonData, CharacterData, MousePositionData, MouseWheelData>;

struct Event
{
    const GenericWindow* window;
    EventData data;
};

using EventQueue = std::queue<Event>;

struct InputSystemData
{
    InputContext default_context = InputContext(Opal::StringUtf8("Default"));
    Opal::DynamicArray<Opal::Ref<InputContext>> contexts;
    EventQueue events;

    ~InputSystemData() = default;
};
}  // namespace Rndr

// InputAction /////////////////////////////////////////////////////////////////////////////////////

Rndr::InputAction::InputAction(Opal::StringUtf8 name, const Opal::DynamicArray<InputBinding>& bindings, const GenericWindow* window)
    : m_name(std::move(name)), m_window(window), m_bindings(bindings)
{
}

const Opal::StringUtf8& Rndr::InputAction::GetName() const
{
    return m_name;
}

const Rndr::GenericWindow* Rndr::InputAction::GetWindow() const
{
    return m_window;
}

const Opal::DynamicArray<Rndr::InputBinding>& Rndr::InputAction::GetBindings() const
{
    return m_bindings;
}

bool Rndr::InputAction::IsValid() const
{
    return !m_name.IsEmpty();
}

// InputBinding ////////////////////////////////////////////////////////////////////////////////

Rndr::InputBinding::InputBinding(const InputBinding& other) : primitive(other.primitive), modifier(other.modifier), trigger(other.trigger)
{
    button_callback = other.button_callback;
}

Rndr::InputBinding& Rndr::InputBinding::operator=(const InputBinding& other)
{
    primitive = other.primitive;
    trigger = other.trigger;
    modifier = other.modifier;
    button_callback = other.button_callback;
    return *this;
}

Rndr::InputBinding Rndr::InputBinding::CreateKeyboardButtonBinding(InputPrimitive button_primitive, InputTrigger trigger,
                                                                   InputButtonCallback callback, f32 modifier)
{
    RNDR_ASSERT(InputSystem::IsKeyboardButton(button_primitive), "Only keyboard button primitives are supported!");
    RNDR_ASSERT(trigger == InputTrigger::ButtonPressed || trigger == InputTrigger::ButtonReleased, "Invalid trigger!");
    RNDR_ASSERT(callback != nullptr, "Callback can't be null!");

    InputBinding binding;
    binding.primitive = button_primitive;
    binding.trigger = trigger;
    binding.button_callback = Opal::Move(callback);
    binding.modifier = modifier;
    return binding;
}

Rndr::InputBinding Rndr::InputBinding::CreateMouseButtonBinding(InputPrimitive button_primitive, InputTrigger trigger,
                                                                InputMouseButtonCallback callback, f32 modifier)
{
    RNDR_ASSERT(InputSystem::IsMouseButton(button_primitive), "Only mouse button primitives are supported!");
    RNDR_ASSERT(
        trigger == InputTrigger::ButtonPressed || trigger == InputTrigger::ButtonReleased || trigger == InputTrigger::ButtonDoubleClick,
        "Invalid trigger!");
    RNDR_ASSERT(callback != nullptr, "Callback can't be null!");

    InputBinding binding;
    binding.primitive = button_primitive;
    binding.trigger = trigger;
    binding.mouse_button_callback = Opal::Move(callback);
    binding.modifier = modifier;
    return binding;
}

Rndr::InputBinding Rndr::InputBinding::CreateTextBinding(InputTextCallback callback)
{
    RNDR_ASSERT(callback != nullptr, "Callback can't be null!");

    InputBinding binding;
    binding.primitive = InputPrimitive::Invalid;
    binding.trigger = InputTrigger::TextCharacter;
    binding.text_callback = Opal::Move(callback);
    return binding;
}

Rndr::InputBinding Rndr::InputBinding::CreateMouseWheelBinding(InputMouseWheelCallback callback, f32 modifier)
{
    RNDR_ASSERT(callback != nullptr, "Callback can't be null!");

    InputBinding binding;
    binding.primitive = InputPrimitive::Mouse_AxisWheel;
    binding.trigger = InputTrigger::AxisChangedRelative;
    binding.mouse_wheel_callback = Opal::Move(callback);
    binding.modifier = modifier;
    return binding;
}

Rndr::InputBinding Rndr::InputBinding::CreateMousePositionBinding(InputPrimitive mouse_axis, InputMousePositionCallback callback,
                                                                  f32 modifier)
{
    RNDR_ASSERT(InputSystem::IsAxis(mouse_axis), "Only mouse axis inputs are supported!");
    RNDR_ASSERT(callback != nullptr, "Callback can't be null!");

    InputBinding binding;
    binding.primitive = mouse_axis;
    binding.trigger = InputTrigger::AxisChangedRelative;
    binding.mouse_position_callback = Opal::Move(callback);
    binding.modifier = modifier;
    return binding;
}

bool Rndr::InputBinding::operator==(const Rndr::InputBinding& other) const
{
    return primitive == other.primitive && trigger == other.trigger;
}

// InputContext ////////////////////////////////////////////////////////////////////////////////////

Rndr::InputContext::InputContext(Opal::StringUtf8 name) : m_name(std::move(name)) {}

Rndr::InputContext::~InputContext() = default;

const Opal::StringUtf8& Rndr::InputContext::GetName() const
{
    return m_name;
}

bool Rndr::InputContext::AddAction(const InputAction& action)
{
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return false;
    }
    m_actions.PushBack(action);
    return true;
}

bool Rndr::InputContext::AddAction(const Opal::StringUtf8& name, const Opal::DynamicArray<InputBinding>& bindings,
                                   const GenericWindow* window)
{
    const InputAction action(name, bindings, window);
    return AddAction(action);
}

bool Rndr::InputContext::ContainsAction(const Opal::StringUtf8& name) const
{
    for (const InputAction& action : m_actions)
    {
        if (action.GetName() == name)
        {
            return true;
        }
    }
    return false;
}

static Rndr::InputAction g_invalid_action;

Rndr::InputAction& Rndr::InputContext::GetAction(const Opal::StringUtf8& name)
{
    for (InputAction& action : m_actions)
    {
        if (action.GetName() == name)
        {
            return action;
        }
    }
    return g_invalid_action;
}

const Rndr::InputAction& Rndr::InputContext::GetAction(const Opal::StringUtf8& name) const
{
    for (const InputAction& action : m_actions)
    {
        if (action.GetName() == name)
        {
            return action;
        }
    }
    return g_invalid_action;
}

const Opal::DynamicArray<Rndr::InputAction>& Rndr::InputContext::GetActions() const
{
    return m_actions;
}

// InputSystem ////////////////////////////////////////////////////////////////////////////////////

namespace
{
Opal::ScopePtr<Rndr::InputSystem> g_input_system;
Rndr::EventQueue g_event_queue;
}

Rndr::InputSystem* Rndr::InputSystem::Get()
{
    if (!g_input_system.IsValid())
    {
        g_input_system = Opal::MakeDefaultScoped<InputSystem>();
    }
    return g_input_system.Get();
}

Rndr::InputSystem& Rndr::InputSystem::GetChecked()
{
    if (!g_input_system.IsValid())
    {
        g_input_system = Opal::MakeDefaultScoped<InputSystem>();
    }
    RNDR_ASSERT(g_input_system.IsValid(), "Rndr::InputSystem::GetChecked()");
    return *g_input_system;
}

bool Rndr::InputSystem::Init()
{
    m_default_context = InputContext("Default Input Context");
    m_contexts.PushBack(Opal::Ref{&m_default_context});
    return true;
}

bool Rndr::InputSystem::Destroy()
{
    m_contexts.Clear();
    return true;
}

Opal::DynamicArray<Opal::Ref<Rndr::InputContext>>& Rndr::InputSystem::GetInputContexts()
{
    return m_contexts;
}

const Opal::DynamicArray<Opal::Ref<Rndr::InputContext>>& Rndr::InputSystem::GetInputContexts() const
{
    return m_contexts;
}

Rndr::InputContext& Rndr::InputSystem::GetCurrentContext()
{
    return m_contexts.Back().GetValue().Get();
}

bool Rndr::InputSystem::PushContext(const Opal::Ref<InputContext>& context)
{
    m_contexts.PushBack(context);
    return true;
}

bool Rndr::InputSystem::PopContext()
{
    if (m_contexts.GetSize() == 1)
    {
        RNDR_LOG_ERROR("Cannot pop default context");
        return false;
    }
    m_contexts.PopBack();
    return true;
}

bool Rndr::InputSystem::OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated)
{
    const Event event{.window = &window,
                      .data = ButtonData{.primitive = key_code, .trigger = InputTrigger::ButtonPressed, .is_repeated = is_repeated}};
    g_event_queue.push(event);
    return true;
}

bool Rndr::InputSystem::OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated)
{
    const Event event{.window = &window,
                      .data = ButtonData{.primitive = key_code, .trigger = InputTrigger::ButtonReleased, .is_repeated = is_repeated}};
    g_event_queue.push(event);
    return true;
}

bool Rndr::InputSystem::OnCharacter(const GenericWindow& window, uchar32 character, bool)
{
    const Event event{.window = &window, .data = CharacterData{.character = character}};
    g_event_queue.push(event);
    return true;
}

bool Rndr::InputSystem::OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    const Event event{
        .window = &window,
        .data = MouseButtonData{.primitive = primitive, .trigger = InputTrigger::ButtonPressed, .cursor_position = cursor_position}};
    g_event_queue.push(event);
    return true;
}

bool Rndr::InputSystem::OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    const Event event{
        .window = &window,
        .data = MouseButtonData{.primitive = primitive, .trigger = InputTrigger::ButtonReleased, .cursor_position = cursor_position}};
    g_event_queue.push(event);
    return true;
}

bool Rndr::InputSystem::OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    const Event event{
        .window = &window,
        .data = MouseButtonData{.primitive = primitive, .trigger = InputTrigger::ButtonDoubleClick, .cursor_position = cursor_position}};
    g_event_queue.push(event);
    return true;
}

bool Rndr::InputSystem::OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position)
{
    const Event event{.window = &window, .data = MouseWheelData{.delta_wheel = wheel_delta, .cursor_position = cursor_position}};
    g_event_queue.push(event);
    return true;
}

bool Rndr::InputSystem::OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y)
{
    const Event event{.window = &window, .data = MousePositionData{Vector2f{delta_x, delta_y}}};
    g_event_queue.push(event);
    return true;
}

namespace Rndr
{
struct InputEventProcessor
{
    Opal::Ref<InputContext> context;
    const GenericWindow* window;

    bool operator()(const ButtonData& event) const;
    bool operator()(const MouseButtonData& event) const;
    bool operator()(const CharacterData& event) const;
    bool operator()(const MousePositionData& event) const;
    bool operator()(const MouseWheelData& event) const;
};

}  // namespace Rndr

bool Rndr::InputSystem::ProcessEvents(float delta_seconds)
{
    RNDR_UNUSED(delta_seconds);

    while (!g_event_queue.empty())
    {
        auto [window, data] = g_event_queue.front();
        g_event_queue.pop();
        for (auto it = m_contexts.end() - 1; it != m_contexts.begin() - 1; --it)
        {
            const Opal::Ref<InputContext>& context = *it;
            if (!context->IsEnabled())
            {
                continue;
            }
            if (std::visit(InputEventProcessor{.context = Opal::Ref(context), .window = window}, data))
            {
                break;
            }
        }
    }
    return true;
}

bool Rndr::InputEventProcessor::operator()(const ButtonData& event) const
{
    for (const InputAction& action : context->GetActions())
    {
        if (action.GetWindow() != nullptr && action.GetWindow() != window)
        {
            continue;
        }
        for (const InputBinding& binding : action.GetBindings())
        {
            if (binding.primitive == event.primitive && binding.trigger == event.trigger)
            {
                binding.button_callback(event.primitive, event.trigger, binding.modifier, event.is_repeated);
                return true;
            }
        }
    }
    return false;
}

bool Rndr::InputEventProcessor::operator()(const MouseButtonData& event) const
{
    for (const InputAction& action : context->GetActions())
    {
        if (action.GetWindow() != nullptr && action.GetWindow() != window)
        {
            continue;
        }
        for (const InputBinding& binding : action.GetBindings())
        {
            if (binding.primitive == event.primitive && binding.trigger == event.trigger)
            {
                binding.mouse_button_callback(event.primitive, event.trigger, binding.modifier, event.cursor_position);
                return true;
            }
        }
    }
    return false;
}

bool Rndr::InputEventProcessor::operator()(const CharacterData& event) const
{
    for (const InputAction& action : context->GetActions())
    {
        if (action.GetWindow() != nullptr && action.GetWindow() != window)
        {
            continue;
        }
        for (const InputBinding& binding : action.GetBindings())
        {
            if (binding.trigger == InputTrigger::TextCharacter)
            {
                binding.text_callback(event.character);
                return true;
            }
        }
    }
    return false;
}

bool Rndr::InputEventProcessor::operator()(const MousePositionData& event) const
{
    const Opal::InPlaceArray<InputPrimitive, 2> axes = {InputPrimitive::Mouse_AxisX, InputPrimitive::Mouse_AxisY};
    for (const InputAction& action : context->GetActions())
    {
        if (action.GetWindow() != nullptr && action.GetWindow() != window)
        {
            continue;
        }
        for (const InputBinding& binding : action.GetBindings())
        {
            bool consumed = false;
            if (binding.primitive == axes[0] && binding.trigger == InputTrigger::AxisChangedRelative && event.delta_position[0] != 0.0f)
            {
                const float value = binding.modifier * event.delta_position[0];
                binding.mouse_position_callback(axes[0], value);
                consumed = true;
            }
            if (binding.primitive == axes[1] && binding.trigger == InputTrigger::AxisChangedRelative && event.delta_position[1] != 0.0f)
            {
                const float value = binding.modifier * event.delta_position[1];
                binding.mouse_position_callback(axes[1], value);
                consumed = true;
            }
            if (consumed)
            {
                return true;
            }
        }
    }
    return false;
}

bool Rndr::InputEventProcessor::operator()(const MouseWheelData& event) const
{
    for (const InputAction& action : context->GetActions())
    {
        if (action.GetWindow() != nullptr && action.GetWindow() != window)
        {
            continue;
        }
        for (const InputBinding& binding : action.GetBindings())
        {
            if (binding.primitive == InputPrimitive::Mouse_AxisWheel && binding.trigger == InputTrigger::AxisChangedRelative)
            {
                const float value = binding.modifier * event.delta_wheel;
                binding.mouse_wheel_callback(value);
                return true;
            }
        }
    }
    return false;
}

bool Rndr::InputSystem::IsButton(InputPrimitive primitive)
{
    return IsKeyboardButton(primitive) || IsMouseButton(primitive);
}

bool Rndr::InputSystem::IsMouseButton(InputPrimitive primitive)
{
    using enum Rndr::InputPrimitive;
    return primitive == Mouse_LeftButton || primitive == Mouse_RightButton || primitive == Mouse_MiddleButton ||
           primitive == Mouse_XButton1 || primitive == Mouse_XButton2;
}

bool Rndr::InputSystem::IsKeyboardButton(InputPrimitive primitive)

{
    return primitive >= InputPrimitive::Backspace && primitive <= InputPrimitive::Apostrophe;
}

bool Rndr::InputSystem::IsAxis(InputPrimitive primitive)
{
    return primitive == InputPrimitive::Mouse_AxisX || primitive == InputPrimitive::Mouse_AxisY;
}

bool Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive primitive)
{
    return primitive == InputPrimitive::Mouse_AxisWheel;
}
