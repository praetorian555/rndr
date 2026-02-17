#include <utility>

#include "opal/container/ref.h"
#include "opal/exceptions.h"

#include "rndr/input-system.hpp"
#include "rndr/log.hpp"

// InputAction::Binding ///////////////////////////////////////////////////////////////////////////

Rndr::InputAction::Binding::Binding() : type(BindingType::Key), key{} {}

Rndr::InputAction::Binding::Binding(const Binding& other) : type(other.type)
{
    switch (type)
    {
        case BindingType::Key:
            key = other.key;
            break;
        case BindingType::MouseButton:
            mouse_button = other.mouse_button;
            break;
        case BindingType::MouseAxis:
            mouse_axis = other.mouse_axis;
            break;
        case BindingType::GamepadButton:
            gamepad_button = other.gamepad_button;
            break;
        case BindingType::GamepadAxis:
            gamepad_axis = other.gamepad_axis;
            break;
        case BindingType::Hold:
            hold = other.hold;
            break;
        case BindingType::Combo:
            new (&combo) ComboBinding(other.combo);
            break;
        case BindingType::Text:
            break;
    }
}

Rndr::InputAction::Binding& Rndr::InputAction::Binding::operator=(const Binding& other)
{
    if (this == &other)
    {
        return *this;
    }
    // Destroy current combo if active.
    if (type == BindingType::Combo)
    {
        combo.~ComboBinding();
    }
    type = other.type;
    switch (type)
    {
        case BindingType::Key:
            key = other.key;
            break;
        case BindingType::MouseButton:
            mouse_button = other.mouse_button;
            break;
        case BindingType::MouseAxis:
            mouse_axis = other.mouse_axis;
            break;
        case BindingType::GamepadButton:
            gamepad_button = other.gamepad_button;
            break;
        case BindingType::GamepadAxis:
            gamepad_axis = other.gamepad_axis;
            break;
        case BindingType::Hold:
            hold = other.hold;
            break;
        case BindingType::Combo:
            new (&combo) ComboBinding(other.combo);
            break;
        case BindingType::Text:
            break;
    }
    return *this;
}

Rndr::InputAction::Binding::Binding(Binding&& other) noexcept : type(other.type)
{
    switch (type)
    {
        case BindingType::Key:
            key = other.key;
            break;
        case BindingType::MouseButton:
            mouse_button = other.mouse_button;
            break;
        case BindingType::MouseAxis:
            mouse_axis = other.mouse_axis;
            break;
        case BindingType::GamepadButton:
            gamepad_button = other.gamepad_button;
            break;
        case BindingType::GamepadAxis:
            gamepad_axis = other.gamepad_axis;
            break;
        case BindingType::Hold:
            hold = other.hold;
            break;
        case BindingType::Combo:
            new (&combo) ComboBinding(std::move(other.combo));
            break;
        case BindingType::Text:
            break;
    }
}

Rndr::InputAction::Binding& Rndr::InputAction::Binding::operator=(Binding&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    if (type == BindingType::Combo)
    {
        combo.~ComboBinding();
    }
    type = other.type;
    switch (type)
    {
        case BindingType::Key:
            key = other.key;
            break;
        case BindingType::MouseButton:
            mouse_button = other.mouse_button;
            break;
        case BindingType::MouseAxis:
            mouse_axis = other.mouse_axis;
            break;
        case BindingType::GamepadButton:
            gamepad_button = other.gamepad_button;
            break;
        case BindingType::GamepadAxis:
            gamepad_axis = other.gamepad_axis;
            break;
        case BindingType::Hold:
            hold = other.hold;
            break;
        case BindingType::Combo:
            new (&combo) ComboBinding(std::move(other.combo));
            break;
        case BindingType::Text:
            break;
    }
    return *this;
}

Rndr::InputAction::Binding::~Binding()
{
    if (type == BindingType::Combo)
    {
        combo.~ComboBinding();
    }
}

// InputAction ////////////////////////////////////////////////////////////////////////////////////

Rndr::InputAction::InputAction(InputAction&& other) noexcept
    : m_name(std::move(other.m_name)),
      m_window(other.m_window),
      m_callback_flags(other.m_callback_flags),
      m_bindings(std::move(other.m_bindings)),
      m_button_callback(std::move(other.m_button_callback)),
      m_mouse_button_callback(std::move(other.m_mouse_button_callback)),
      m_mouse_position_callback(std::move(other.m_mouse_position_callback)),
      m_mouse_wheel_callback(std::move(other.m_mouse_wheel_callback)),
      m_gamepad_button_callback(std::move(other.m_gamepad_button_callback)),
      m_gamepad_axis_callback(std::move(other.m_gamepad_axis_callback)),
      m_text_callback(std::move(other.m_text_callback))
{
    other.m_window = nullptr;
    other.m_callback_flags = CallbackFlags::None;
}

Rndr::InputAction& Rndr::InputAction::operator=(InputAction&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    m_name = std::move(other.m_name);
    m_window = other.m_window;
    m_callback_flags = other.m_callback_flags;
    m_bindings = std::move(other.m_bindings);
    m_button_callback = std::move(other.m_button_callback);
    m_mouse_button_callback = std::move(other.m_mouse_button_callback);
    m_mouse_position_callback = std::move(other.m_mouse_position_callback);
    m_mouse_wheel_callback = std::move(other.m_mouse_wheel_callback);
    m_gamepad_button_callback = std::move(other.m_gamepad_button_callback);
    m_gamepad_axis_callback = std::move(other.m_gamepad_axis_callback);
    m_text_callback = std::move(other.m_text_callback);
    other.m_window = nullptr;
    other.m_callback_flags = CallbackFlags::None;
    return *this;
}

const Opal::StringUtf8& Rndr::InputAction::GetName() const
{
    return m_name;
}

const Rndr::GenericWindow* Rndr::InputAction::GetWindow() const
{
    return m_window;
}

void Rndr::InputAction::SetWindow(const GenericWindow* window)
{
    m_window = window;
}

// InputActionBuilder /////////////////////////////////////////////////////////////////////////////

Rndr::InputActionBuilder::InputActionBuilder(InputAction& action) : m_action(action) {}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::OnButton(ButtonCallback callback)
{
    m_action->m_callback_flags |= InputAction::CallbackFlags::Button;
    m_action->m_button_callback = std::move(callback);
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::OnMouseButton(MouseButtonCallback callback)
{
    m_action->m_callback_flags |= InputAction::CallbackFlags::MouseButton;
    m_action->m_mouse_button_callback = std::move(callback);
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::OnMousePosition(MousePositionCallback callback)
{
    m_action->m_callback_flags |= InputAction::CallbackFlags::MousePosition;
    m_action->m_mouse_position_callback = std::move(callback);
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::OnMouseWheel(MouseWheelCallback callback)
{
    m_action->m_callback_flags |= InputAction::CallbackFlags::MouseWheel;
    m_action->m_mouse_wheel_callback = std::move(callback);
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::OnGamepadButton(GamepadButtonCallback callback)
{
    m_action->m_callback_flags |= InputAction::CallbackFlags::GamepadButton;
    m_action->m_gamepad_button_callback = std::move(callback);
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::OnGamepadAxis(GamepadAxisCallback callback)
{
    m_action->m_callback_flags |= InputAction::CallbackFlags::GamepadAxis;
    m_action->m_gamepad_axis_callback = std::move(callback);
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::OnText(TextCallback callback)
{
    m_action->m_callback_flags |= InputAction::CallbackFlags::Text;
    m_action->m_text_callback = std::move(callback);
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::Bind(Key key, Trigger trigger, Modifiers modifiers)
{
    InputAction::Binding binding;
    binding.type = InputAction::BindingType::Key;
    binding.key = InputAction::KeyBinding{.key = key, .trigger = trigger, .modifiers = modifiers};
    m_action->m_bindings.PushBack(std::move(binding));
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::Bind(MouseButton button, Trigger trigger)
{
    InputAction::Binding binding;
    binding.type = InputAction::BindingType::MouseButton;
    binding.mouse_button = InputAction::MouseButtonBinding{.button = button, .trigger = trigger};
    m_action->m_bindings.PushBack(std::move(binding));
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::Bind(MouseAxis axis)
{
    InputAction::Binding binding;
    binding.type = InputAction::BindingType::MouseAxis;
    binding.mouse_axis = InputAction::MouseAxisBinding{.axis = axis};
    m_action->m_bindings.PushBack(std::move(binding));
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::Bind(GamepadButton button, Trigger trigger, u8 gamepad_index)
{
    InputAction::Binding binding;
    binding.type = InputAction::BindingType::GamepadButton;
    binding.gamepad_button = InputAction::GamepadButtonBinding{.button = button, .trigger = trigger, .gamepad_index = gamepad_index};
    m_action->m_bindings.PushBack(std::move(binding));
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::Bind(GamepadAxis axis, f32 dead_zone, u8 gamepad_index)
{
    InputAction::Binding binding;
    binding.type = InputAction::BindingType::GamepadAxis;
    binding.gamepad_axis = InputAction::GamepadAxisBinding{.axis = axis, .dead_zone = dead_zone, .gamepad_index = gamepad_index};
    m_action->m_bindings.PushBack(std::move(binding));
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::BindHold(Key key, f32 duration_seconds)
{
    RNDR_ASSERT(duration_seconds > 0.0f, "Hold duration must be positive");
    InputAction::Binding binding;
    binding.type = InputAction::BindingType::Hold;
    binding.hold = InputAction::HoldBinding{.key = key, .duration_seconds = duration_seconds};
    m_action->m_bindings.PushBack(std::move(binding));
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::BindCombo(Opal::ArrayView<Key> keys, f32 timeout_seconds)
{
    RNDR_ASSERT(keys.GetSize() >= 2, "Combo must have at least 2 keys");
    RNDR_ASSERT(timeout_seconds >= 0.0f, "Combo timeout must be non-negative");

    InputAction::Binding binding;
    binding.type = InputAction::BindingType::Combo;
    new (&binding.combo) InputAction::ComboBinding();
    for (const Key& k : keys)
    {
        binding.combo.keys.PushBack(k);
    }
    binding.combo.timeout_seconds = timeout_seconds;
    m_action->m_bindings.PushBack(std::move(binding));
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::BindText()
{
    InputAction::Binding binding;
    binding.type = InputAction::BindingType::Text;
    m_action->m_bindings.PushBack(std::move(binding));
    return *this;
}

Rndr::InputActionBuilder& Rndr::InputActionBuilder::ForWindow(const GenericWindow* window)
{
    m_action->m_window = window;
    return *this;
}

// InputContext ///////////////////////////////////////////////////////////////////////////////////

Rndr::InputContext::InputContext(Opal::StringUtf8 name) : m_name(std::move(name)) {}

Rndr::InputContext::~InputContext() = default;

Rndr::InputContext::InputContext(InputContext&& other) noexcept
    : m_name(std::move(other.m_name)), m_actions(std::move(other.m_actions)), m_enabled(other.m_enabled)
{
}

Rndr::InputContext& Rndr::InputContext::operator=(InputContext&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    m_name = std::move(other.m_name);
    m_actions = std::move(other.m_actions);
    m_enabled = other.m_enabled;
    return *this;
}

const Opal::StringUtf8& Rndr::InputContext::GetName() const
{
    return m_name;
}

Rndr::InputActionBuilder Rndr::InputContext::AddAction(const Opal::StringUtf8& name)
{
    if (ContainsAction(name))
    {
        throw Opal::Exception("Duplicate action name");
    }
    InputAction action;
    action.m_name = name;
    m_actions.PushBack(std::move(action));
    return InputActionBuilder(m_actions.Back());
}

bool Rndr::InputContext::RemoveAction(const Opal::StringUtf8& name)
{
    for (auto it = m_actions.begin(); it != m_actions.end(); ++it)
    {
        if (it->GetName() == name)
        {
            m_actions.Erase(it);
            return true;
        }
    }
    return false;
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

Rndr::InputAction* Rndr::InputContext::GetAction(const Opal::StringUtf8& name)
{
    for (InputAction& action : m_actions)
    {
        if (action.GetName() == name)
        {
            return &action;
        }
    }
    return nullptr;
}

const Rndr::InputAction* Rndr::InputContext::GetAction(const Opal::StringUtf8& name) const
{
    for (const InputAction& action : m_actions)
    {
        if (action.GetName() == name)
        {
            return &action;
        }
    }
    return nullptr;
}

bool Rndr::InputContext::IsEnabled() const
{
    return m_enabled;
}

void Rndr::InputContext::SetEnabled(bool enabled)
{
    m_enabled = enabled;
}

// InputSystem ////////////////////////////////////////////////////////////////////////////////////

Rndr::InputSystem::InputSystem()
    : m_default_context(Opal::StringUtf8("Default"))
{
    m_contexts.PushBack(Opal::Ref<InputContext>(m_default_context));
}

Rndr::InputSystem::~InputSystem() = default;

void Rndr::InputSystem::SetDefaultDeadZone(f32 dead_zone)
{
    m_default_dead_zone = dead_zone;
}

Rndr::f32 Rndr::InputSystem::GetDefaultDeadZone() const
{
    return m_default_dead_zone;
}

Rndr::InputContext& Rndr::InputSystem::GetCurrentContext()
{
    return m_contexts.Back().Get();
}

const Rndr::InputContext& Rndr::InputSystem::GetCurrentContext() const
{
    return m_contexts.Back().Get();
}

Rndr::InputContext& Rndr::InputSystem::GetContextByName(const Opal::StringUtf8& name)
{
    for (auto& context : m_contexts)
    {
        if (context->GetName() == name)
        {
            return context;
        }
    }
    throw Opal::Exception("Context by the given name does not exist!");
}

const Rndr::InputContext& Rndr::InputSystem::GetContextByName(const Opal::StringUtf8& name) const
{
    for (const auto& context : m_contexts)
    {
        if (context->GetName() == name)
        {
            return context;
        }
    }
    throw Opal::Exception("Context by the given name does not exist!");
}

void Rndr::InputSystem::PushContext(InputContext& context)
{
    m_contexts.PushBack(Opal::Ref<InputContext>(context));
}

bool Rndr::InputSystem::PopContext()
{
    if (m_contexts.GetSize() <= 1)
    {
        RNDR_LOG_ERROR("Cannot pop default context");
        return false;
    }
    m_contexts.PopBack();
    return true;
}

void Rndr::InputSystem::ProcessSystemEvents(f32 delta_seconds)
{
    DispatchEvents();
    UpdateTimers(delta_seconds);
}

// SystemMessageHandler overrides /////////////////////////////////////////////////////////////////

bool Rndr::InputSystem::OnWindowClose(GenericWindow& /*window*/)
{
    return false;
}

void Rndr::InputSystem::OnWindowSizeChanged(const GenericWindow& /*window*/, i32 /*width*/, i32 /*height*/)
{
}

bool Rndr::InputSystem::OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated)
{
    if (!IsKeyboardPrimitive(key_code))
    {
        return false;
    }
    const Key key = InputPrimitiveToKey(key_code);

    // Track modifier state.
    if (key == Key::LeftCtrl || key == Key::RightCtrl || key == Key::Control)
    {
        m_current_modifiers |= Modifiers::Ctrl;
    }
    if (key == Key::LeftShift || key == Key::RightShift || key == Key::Shift)
    {
        m_current_modifiers |= Modifiers::Shift;
    }
    if (key == Key::LeftAlt || key == Key::RightAlt || key == Key::Alt)
    {
        m_current_modifiers |= Modifiers::Alt;
    }

    m_event_queue.PushBack(KeyEvent{
        .window = &window,
        .key = key,
        .trigger = Trigger::Pressed,
        .is_repeated = is_repeated,
    });
    return true;
}

bool Rndr::InputSystem::OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated)
{
    if (!IsKeyboardPrimitive(key_code))
    {
        return false;
    }
    const Key key = InputPrimitiveToKey(key_code);

    // Track modifier state.
    if (key == Key::LeftCtrl || key == Key::RightCtrl || key == Key::Control)
    {
        m_current_modifiers &= ~Modifiers::Ctrl;
    }
    if (key == Key::LeftShift || key == Key::RightShift || key == Key::Shift)
    {
        m_current_modifiers &= ~Modifiers::Shift;
    }
    if (key == Key::LeftAlt || key == Key::RightAlt || key == Key::Alt)
    {
        m_current_modifiers &= ~Modifiers::Alt;
    }

    m_event_queue.PushBack(KeyEvent{
        .window = &window,
        .key = key,
        .trigger = Trigger::Released,
        .is_repeated = is_repeated,
    });
    return true;
}

bool Rndr::InputSystem::OnCharacter(const GenericWindow& window, uchar32 character, bool /*is_repeated*/)
{
    m_event_queue.PushBack(CharacterEvent{
        .window = &window,
        .character = character,
    });
    return true;
}

bool Rndr::InputSystem::OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    if (!IsMouseButtonPrimitive(primitive))
    {
        return false;
    }
    m_event_queue.PushBack(MouseButtonEvent{
        .window = &window,
        .button = InputPrimitiveToMouseButton(primitive),
        .trigger = Trigger::Pressed,
        .cursor_position = cursor_position,
    });
    return true;
}

bool Rndr::InputSystem::OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    if (!IsMouseButtonPrimitive(primitive))
    {
        return false;
    }
    m_event_queue.PushBack(MouseButtonEvent{
        .window = &window,
        .button = InputPrimitiveToMouseButton(primitive),
        .trigger = Trigger::Released,
        .cursor_position = cursor_position,
    });
    return true;
}

bool Rndr::InputSystem::OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position)
{
    // Treat double click as a press event. Actions that care about double-click can be added later.
    return OnMouseButtonDown(window, primitive, cursor_position);
}

bool Rndr::InputSystem::OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position)
{
    m_event_queue.PushBack(MouseWheelEvent{
        .window = &window,
        .delta_wheel = wheel_delta,
        .cursor_position = cursor_position,
    });
    return true;
}

bool Rndr::InputSystem::OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y)
{
    m_event_queue.PushBack(MouseMoveEvent{
        .window = &window,
        .delta_x = delta_x,
        .delta_y = delta_y,
    });
    return true;
}

// Event dispatching //////////////////////////////////////////////////////////////////////////////

void Rndr::InputSystem::DispatchEvents()
{
    for (const InputEvent& event : m_event_queue)
    {
        // Walk contexts from top to bottom.
        for (auto it = m_contexts.end() - 1; it >= m_contexts.begin(); --it)
        {
            InputContext& context = it->Get();
            if (!context.IsEnabled())
            {
                continue;
            }

            bool consumed = false;

            for (InputAction& action : context.m_actions)
            {
                if (std::holds_alternative<KeyEvent>(event))
                {
                    consumed = DispatchKeyEvent(std::get<KeyEvent>(event), action);
                }
                else if (std::holds_alternative<MouseButtonEvent>(event))
                {
                    consumed = DispatchMouseButtonEvent(std::get<MouseButtonEvent>(event), action);
                }
                else if (std::holds_alternative<MouseMoveEvent>(event))
                {
                    consumed = DispatchMouseMoveEvent(std::get<MouseMoveEvent>(event), action);
                }
                else if (std::holds_alternative<MouseWheelEvent>(event))
                {
                    consumed = DispatchMouseWheelEvent(std::get<MouseWheelEvent>(event), action);
                }
                else if (std::holds_alternative<CharacterEvent>(event))
                {
                    consumed = DispatchCharacterEvent(std::get<CharacterEvent>(event), action);
                }

                if (consumed)
                {
                    break;
                }
            }

            if (consumed)
            {
                break;
            }
        }
    }
    m_event_queue.Clear();
}

void Rndr::InputSystem::UpdateTimers(f32 delta_seconds)
{
    // Walk contexts from top to bottom.
    for (auto it = m_contexts.end() - 1; it >= m_contexts.begin(); --it)
    {
        InputContext& context = it->Get();
        if (!context.IsEnabled())
        {
            continue;
        }

        for (InputAction& action : context.m_actions)
        {
            for (InputAction::Binding& binding : action.m_bindings)
            {

                if (binding.type == InputAction::BindingType::Hold)
                {
                    InputAction::HoldBinding& hold = binding.hold;
                    if (hold.is_held && !hold.has_fired)
                    {
                        hold.elapsed_seconds += delta_seconds;
                        if (hold.elapsed_seconds >= hold.duration_seconds)
                        {
                            hold.has_fired = true;
                            if (!!(action.m_callback_flags & InputAction::CallbackFlags::Button) && action.m_button_callback)
                            {
                                action.m_button_callback(Trigger::Pressed, false);
                            }
                        }
                    }
                }
                else if (binding.type == InputAction::BindingType::Combo)
                {
                    InputAction::ComboBinding& combo = binding.combo;
                    if (combo.current_step > 0 && combo.timeout_seconds > 0.0f)
                    {
                        combo.elapsed_since_last_step += delta_seconds;
                        if (combo.elapsed_since_last_step > combo.timeout_seconds)
                        {
                            // Combo timed out, reset.
                            combo.current_step = 0;
                            combo.elapsed_since_last_step = 0.0f;
                        }
                    }
                }
            }
        }
    }
}

// Per-event-type dispatch helpers /////////////////////////////////////////////////////////////////

bool Rndr::InputSystem::DispatchKeyEvent(const KeyEvent& event, InputAction& action)
{
    if (action.m_window != nullptr && action.m_window != event.window)
    {
        return false;
    }

    for (InputAction::Binding& binding : action.m_bindings)
    {

        if (binding.type == InputAction::BindingType::Key)
        {
            const InputAction::KeyBinding& kb = binding.key;
            if (kb.key == event.key && kb.trigger == event.trigger && kb.modifiers == (m_current_modifiers & kb.modifiers))
            {
                if (!!(action.m_callback_flags & InputAction::CallbackFlags::Button) && action.m_button_callback)
                {
                    action.m_button_callback(event.trigger, event.is_repeated);
                    return true;
                }
            }
        }
        else if (binding.type == InputAction::BindingType::Hold)
        {
            InputAction::HoldBinding& hold = binding.hold;
            if (hold.key == event.key)
            {
                if (event.trigger == Trigger::Pressed && !hold.is_held)
                {
                    hold.is_held = true;
                    hold.elapsed_seconds = 0.0f;
                    hold.has_fired = false;
                }
                else if (event.trigger == Trigger::Released)
                {
                    hold.is_held = false;
                    hold.elapsed_seconds = 0.0f;
                    hold.has_fired = false;
                }
            }
        }
        else if (binding.type == InputAction::BindingType::Combo)
        {
            InputAction::ComboBinding& combo = binding.combo;
            if (event.trigger == Trigger::Pressed && combo.keys.GetSize() > 0)
            {
                const i32 step = combo.current_step;
                if (combo.keys[step] == event.key)
                {
                    combo.current_step++;
                    combo.elapsed_since_last_step = 0.0f;

                    if (combo.current_step >= static_cast<i32>(combo.keys.GetSize()))
                    {
                        // Combo completed.
                        combo.current_step = 0;
                        combo.elapsed_since_last_step = 0.0f;
                        if (!!(action.m_callback_flags & InputAction::CallbackFlags::Button) && action.m_button_callback)
                        {
                            action.m_button_callback(Trigger::Pressed, false);
                            return true;
                        }
                    }
                }
                else
                {
                    // Wrong key, reset combo.
                    combo.current_step = 0;
                    combo.elapsed_since_last_step = 0.0f;
                }
            }
        }
    }
    return false;
}

bool Rndr::InputSystem::DispatchMouseButtonEvent(const MouseButtonEvent& event, InputAction& action)
{
    if (action.m_window != nullptr && action.m_window != event.window)
    {
        return false;
    }

    for (const InputAction::Binding& binding : action.m_bindings)
    {
        if (binding.type == InputAction::BindingType::MouseButton)
        {
            const InputAction::MouseButtonBinding& mb = binding.mouse_button;
            if (mb.button == event.button && mb.trigger == event.trigger)
            {
                if (!!(action.m_callback_flags & InputAction::CallbackFlags::MouseButton) && action.m_mouse_button_callback)
                {
                    action.m_mouse_button_callback(event.button, event.trigger, event.cursor_position);
                    return true;
                }
            }
        }
    }
    return false;
}

bool Rndr::InputSystem::DispatchMouseMoveEvent(const MouseMoveEvent& event, InputAction& action)
{
    if (action.m_window != nullptr && action.m_window != event.window)
    {
        return false;
    }

    bool consumed = false;

    for (const InputAction::Binding& binding : action.m_bindings)
    {
        if (binding.type == InputAction::BindingType::MouseAxis)
        {
            if (!!(action.m_callback_flags & InputAction::CallbackFlags::MousePosition) && action.m_mouse_position_callback)
            {
                if (binding.mouse_axis.axis == MouseAxis::X && event.delta_x != 0.0f)
                {
                    action.m_mouse_position_callback(MouseAxis::X, event.delta_x);
                    consumed = true;
                }
                else if (binding.mouse_axis.axis == MouseAxis::Y && event.delta_y != 0.0f)
                {
                    action.m_mouse_position_callback(MouseAxis::Y, event.delta_y);
                    consumed = true;
                }
            }
        }
    }
    return consumed;
}

bool Rndr::InputSystem::DispatchMouseWheelEvent(const MouseWheelEvent& event, InputAction& action)
{
    if (action.m_window != nullptr && action.m_window != event.window)
    {
        return false;
    }

    for (const InputAction::Binding& binding : action.m_bindings)
    {
        if (binding.type == InputAction::BindingType::MouseAxis)
        {
            if (binding.mouse_axis.axis == MouseAxis::WheelY)
            {
                if (!!(action.m_callback_flags & InputAction::CallbackFlags::MouseWheel) && action.m_mouse_wheel_callback)
                {
                    action.m_mouse_wheel_callback(0.0f, event.delta_wheel);
                    return true;
                }
            }
        }
    }
    return false;
}

bool Rndr::InputSystem::DispatchCharacterEvent(const CharacterEvent& event, InputAction& action)
{
    if (action.m_window != nullptr && action.m_window != event.window)
    {
        return false;
    }

    for (const InputAction::Binding& binding : action.m_bindings)
    {
        if (binding.type == InputAction::BindingType::Text)
        {
            if (!!(action.m_callback_flags & InputAction::CallbackFlags::Text) && action.m_text_callback)
            {
                action.m_text_callback(event.character);
                return true;
            }
        }
    }
    return false;
}

// Conversion helpers /////////////////////////////////////////////////////////////////////////////

Rndr::Key Rndr::InputSystem::InputPrimitiveToKey(InputPrimitive primitive)
{
    return static_cast<Key>(static_cast<u16>(primitive));
}

Rndr::MouseButton Rndr::InputSystem::InputPrimitiveToMouseButton(InputPrimitive primitive)
{
    switch (primitive)
    {
        case InputPrimitive::Mouse_LeftButton:
            return MouseButton::Left;
        case InputPrimitive::Mouse_RightButton:
            return MouseButton::Right;
        case InputPrimitive::Mouse_MiddleButton:
            return MouseButton::Middle;
        case InputPrimitive::Mouse_XButton1:
            return MouseButton::X1;
        case InputPrimitive::Mouse_XButton2:
            return MouseButton::X2;
        default:
            RNDR_ASSERT(false, "Invalid mouse button primitive");
            return MouseButton::Left;
    }
}

bool Rndr::InputSystem::IsKeyboardPrimitive(InputPrimitive primitive)
{
    const auto value = static_cast<u16>(primitive);
    return value >= static_cast<u16>(InputPrimitive::Backspace) && value <= static_cast<u16>(InputPrimitive::Apostrophe);
}

bool Rndr::InputSystem::IsMouseButtonPrimitive(InputPrimitive primitive)
{
    return primitive == InputPrimitive::Mouse_LeftButton || primitive == InputPrimitive::Mouse_RightButton ||
           primitive == InputPrimitive::Mouse_MiddleButton || primitive == InputPrimitive::Mouse_XButton1 ||
           primitive == InputPrimitive::Mouse_XButton2;
}
