#pragma once

#if RNDR_OLD_INPUT_SYSTEM

#include <functional>

#include "opal/container/dynamic-array.h"
#include "opal/container/scope-ptr.h"
#include "opal/container/string.h"

#include "rndr/input-primitives.hpp"
#include "rndr/math.hpp"
#include "rndr/system-message-handler.hpp"

namespace Rndr
{

/**
 * Represents a callback that is invoked when an input action is triggered.
 */
using InputButtonCallback = std::function<void(InputPrimitive primitive, InputTrigger trigger, f32 value, bool is_repeated)>;
using InputMouseButtonCallback =
    std::function<void(InputPrimitive primitive, InputTrigger trigger, f32 value, const Vector2i& cursor_position)>;
using InputTextCallback = std::function<void(uchar32 character)>;
using InputMousePositionCallback = std::function<void(InputPrimitive primitive, f32 delta)>;
using InputMouseWheelCallback = std::function<void(f32 delta)>;

/**
 * Represents an input event that can be bound to an input action.
 */
class InputBinding
{
public:
    InputPrimitive primitive = InputPrimitive::Invalid;
    InputTrigger trigger = InputTrigger::ButtonPressed;
    f32 modifier = 1.0f;
    union
    {
        InputButtonCallback button_callback = nullptr;
        InputMouseButtonCallback mouse_button_callback;
        InputMouseWheelCallback mouse_wheel_callback;
        InputMousePositionCallback mouse_position_callback;
        InputTextCallback text_callback;
    };

    InputBinding() {}
    InputBinding(const InputBinding& other);
    InputBinding& operator=(const InputBinding& other);

    ~InputBinding() {}

    static InputBinding CreateKeyboardButtonBinding(InputPrimitive button_primitive, InputTrigger trigger, InputButtonCallback callback,
                                                    f32 modifier = 1.0f);
    static InputBinding CreateMouseButtonBinding(InputPrimitive button_primitive, InputTrigger trigger, InputMouseButtonCallback callback,
                                                 f32 modifier = 1.0f);
    static InputBinding CreateTextBinding(InputTextCallback callback);
    static InputBinding CreateMouseWheelBinding(InputMouseWheelCallback callback, f32 modifier = 1.0f);
    static InputBinding CreateMousePositionBinding(InputPrimitive mouse_axis, InputMousePositionCallback callback, f32 modifier = 1.0f);

    /**
     * Compares two input bindings. The modifier is not considered.
     * @param other The other input binding to compare.
     * @return Returns true if the input bindings are equal, false otherwise.
     */
    bool operator==(const InputBinding& other) const;
};

/**
 * User defined input action.
 */
struct InputAction
{
public:
    InputAction() = default;
    explicit InputAction(Opal::StringUtf8 name, const Opal::DynamicArray<InputBinding>& bindings, const GenericWindow* window = nullptr);

    [[nodiscard]] const Opal::StringUtf8& GetName() const;
    [[nodiscard]] const GenericWindow* GetWindow() const;
    [[nodiscard]] const Opal::DynamicArray<InputBinding>& GetBindings() const;
    [[nodiscard]] bool IsValid() const;

    bool operator==(const InputAction& other) const = default;

private:
    Opal::StringUtf8 m_name;
    const GenericWindow* m_window = nullptr;
    Opal::DynamicArray<InputBinding> m_bindings;
};

/**
 * Represents a collection of mappings between input actions and input bindings and callbacks.
 */
class InputContext
{
public:
    /**
     * Creates a new input context with an empty name.
     */
    InputContext() = default;

    /**
     * Creates a new input context.
     * @param name Name of the input context.
     */
    explicit InputContext(Opal::StringUtf8 name);

    /**
     * Default destructor.
     */
    ~InputContext();

    /**
     * Returns the name of the input context.
     * @return Returns the name of the input context.
     */
    [[nodiscard]] const Opal::StringUtf8& GetName() const;

    bool AddAction(const InputAction& action);

    bool AddAction(const Opal::StringUtf8& name, const Opal::DynamicArray<InputBinding>& bindings, const GenericWindow* window = nullptr);

    /**
     * Checks if the input context contains an input action.
     * @param name The input action's name to check.
     * @return Returns true if the input context contains the input action.
     */
    [[nodiscard]] bool ContainsAction(const Opal::StringUtf8& name) const;

    InputAction& GetAction(const Opal::StringUtf8& name);
    [[nodiscard]] const InputAction& GetAction(const Opal::StringUtf8& name) const;

    const Opal::DynamicArray<InputAction>& GetActions() const;

    [[nodiscard]] bool IsEnabled() const { return m_enabled; }
    void SetEnabled(const bool enabled) { m_enabled = enabled; }

private:
    Opal::StringUtf8 m_name;
    Opal::DynamicArray<InputAction> m_actions;
    bool m_enabled = true;

    // Implementation details. ////////////////////////////////////////////////////////////////////
    friend class InputSystem;
};

/**
 * Represents a system that handles input events.
 *
 * The input system's lifetime is controlled using the Init and Destroy functions. The input system
 * is initialized by the rndr::Init function and destroyed by the rndr::Destroy function. In case
 * that the input system is not initialized all functions will return false.
 *
 * The input system is a stack based system. Each input context is pushed to the top of the stack
 * and becomes the active input context. Only the context on top of the stack will process the input
 * event. The input context lifetime is not managed by the input system.
 *
 * Events are queued using the Submit*Event family of functions. The events are queued, and
 * processed when the ProcessEvents function is called.
 *
 * The system is not thread-safe.
 */
class InputSystem : public SystemMessageHandler
{
public:
    static InputSystem* Get();
    static InputSystem& GetChecked();

    /**
     * Initializes the input system.
     * @return Returns true if the input system was successfully initialized.
     */
    bool Init();

    /**
     * Destroys the input system.
     * @return Returns true if the input system was successfully destroyed.
     */
    bool Destroy();

    Opal::DynamicArray<Opal::Ref<InputContext>>& GetInputContexts();
    const Opal::DynamicArray<Opal::Ref<InputContext>>& GetInputContexts() const;

    /**
     * Returns the currently active input context.
     * @return Returns the input context on top of the stack.
     */
    InputContext& GetCurrentContext();

    /**
     * Pushes a new input context to the top of the stack making it the active context.
     * @param context Context to be pushed to the top of the stack.
     * @note The input system does not manage the input context lifetime.
     */
    bool PushContext(const Opal::Ref<InputContext>& context);

    /**
     * Pops the input context from the top of the stack. This does not destroy the context.
     */
    bool PopContext();

    /**
     * Processes all pending input events.
     * @param delta_seconds Time that has elapsed since the last frame.
     */
    bool ProcessEvents(float delta_seconds);

    bool OnWindowClose(GenericWindow&) override { return false; }
    void OnWindowSizeChanged(const GenericWindow&, i32, i32) override {}
    bool OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnCharacter(const GenericWindow& window, uchar32 character, bool is_repeated) override;
    bool OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position) override;
    bool OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y) override;

    /**
     * Helper function to check if the primitive is a button.
     * @param primitive Input primitive to check.
     * @return Returns true if the primitive is a button.
     */
    static bool IsButton(InputPrimitive primitive);

    /**
     * Helper function to check if the primitive is a mouse button.
     * @param primitive Input primitive to check.
     * @return Returns true if the primitive is a mouse button.
     */
    static bool IsMouseButton(InputPrimitive primitive);

    /**
     * Helper function to check if the primitive is a keyboard button.
     * @param primitive Input primitive to check.
     * @return Returns true if the primitive is a keyboard button.
     */
    static bool IsKeyboardButton(InputPrimitive primitive);

    /**
     * Helper function to check if the primitive is an axis.
     * @param primitive Input primitive to check.
     * @return Returns true if the primitive is an axis.
     */
    static bool IsAxis(InputPrimitive primitive);

    /**
     * Helper function to check if the primitive is a mouse wheel.
     * @param primitive Input primitive to check.
     * @return Returns true if the primitive is a mouse wheel.
     */
    static bool IsMouseWheelAxis(InputPrimitive primitive);

private:
    InputContext m_default_context;
    Opal::DynamicArray<Opal::Ref<InputContext>> m_contexts;
};

/**
 * Helper macro to bind a member function to an input callback.
 */
#define RNDR_BIND_INPUT_BUTTON_CALLBACK(this, func_ptr)                                                                    \
    std::bind(&std::remove_reference<decltype(*this)>::type::func_ptr, this, std::placeholders::_1, std::placeholders::_2, \
              std::placeholders::_3, std::placeholders::_4)

#define RNDR_BIND_INPUT_MOUSE_BUTTON_CALLBACK(this, func_ptr)                                                              \
    std::bind(&std::remove_reference<decltype(*this)>::type::func_ptr, this, std::placeholders::_1, std::placeholders::_2, \
              std::placeholders::_3, std::placeholders::_4)

#define RNDR_BIND_INPUT_TEXT_CALLBACK(this, func_ptr) \
    std::bind(&std::remove_reference<decltype(*this)>::type::func_ptr, this, std::placeholders::_1)

#define RNDR_BIND_INPUT_MOUSE_POSITION_CALLBACK(this, func_ptr) \
    std::bind(&std::remove_reference<decltype(*this)>::type::func_ptr, this, std::placeholders::_1, std::placeholders::_2)

#define RNDR_BIND_INPUT_MOUSE_WHEEL_CALLBACK(this, func_ptr) \
    std::bind(&std::remove_reference<decltype(*this)>::type::func_ptr, this, std::placeholders::_1)

}  // namespace Rndr

#else  // !RNDR_OLD_INPUT_SYSTEM

#include <functional>

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/scope-ptr.h"
#include "opal/container/string.h"
#include "opal/enum-flags.h"

#include "rndr/math.hpp"
#include "rndr/system-message-handler.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

class GenericWindow;
class InputContext;
class InputSystem;
class InputActionBuilder;

// Input Primitive Enums ///////////////////////////////////////////////////////////////////////////

enum class Key : u16
{
    Backspace = 0x08,
    Tab = 0x09,
    Clear = 0x0C,
    Return = 0x0D,
    Shift = 0x10,
    Control = 0x11,
    Alt = 0x12,
    Pause = 0x13,
    CapsLock = 0x14,
    Escape = 0x1B,
    Space = 0x20,
    PageUp = 0x21,
    PageDown = 0x22,
    End = 0x23,
    Home = 0x24,
    LeftArrow = 0x25,
    UpArrow = 0x26,
    RightArrow = 0x27,
    DownArrow = 0x28,
    Insert = 0x2D,
    Delete = 0x2E,
    Digit_0 = 0x30,
    Digit_1 = 0x31,
    Digit_2 = 0x32,
    Digit_3 = 0x33,
    Digit_4 = 0x34,
    Digit_5 = 0x35,
    Digit_6 = 0x36,
    Digit_7 = 0x37,
    Digit_8 = 0x38,
    Digit_9 = 0x39,
    A = 0x41,
    B = 0x42,
    C = 0x43,
    D = 0x44,
    E = 0x45,
    F = 0x46,
    G = 0x47,
    H = 0x48,
    I = 0x49,
    J = 0x4A,
    K = 0x4B,
    L = 0x4C,
    M = 0x4D,
    N = 0x4E,
    O = 0x4F,
    P = 0x50,
    Q = 0x51,
    R = 0x52,
    S = 0x53,
    T = 0x54,
    U = 0x55,
    V = 0x56,
    W = 0x57,
    X = 0x58,
    Y = 0x59,
    Z = 0x5A,
    LeftLogo = 0x5B,
    RightLogo = 0x5C,
    Numpad_0 = 0x60,
    Numpad_1 = 0x61,
    Numpad_2 = 0x62,
    Numpad_3 = 0x63,
    Numpad_4 = 0x64,
    Numpad_5 = 0x65,
    Numpad_6 = 0x66,
    Numpad_7 = 0x67,
    Numpad_8 = 0x68,
    Numpad_9 = 0x69,
    Multiply = 0x6A,
    Add = 0x6B,
    Separator = 0x6C,
    Subtract = 0x6D,
    Decimal = 0x6E,
    Divide = 0x6F,
    F1 = 0x70,
    F2 = 0x71,
    F3 = 0x72,
    F4 = 0x73,
    F5 = 0x74,
    F6 = 0x75,
    F7 = 0x76,
    F8 = 0x77,
    F9 = 0x78,
    F10 = 0x79,
    F11 = 0x7A,
    F12 = 0x7B,
    F13 = 0x7C,
    F14 = 0x7D,
    F15 = 0x7E,
    F16 = 0x7F,
    F17 = 0x80,
    F18 = 0x81,
    F19 = 0x82,
    F20 = 0x83,
    F21 = 0x84,
    F22 = 0x85,
    F23 = 0x86,
    F24 = 0x87,
    NumLock = 0x90,
    ScrollLock = 0x91,
    LeftShift = 0xA0,
    RightShift = 0xA1,
    LeftCtrl = 0xA2,
    RightCtrl = 0xA3,
    LeftAlt = 0xA4,
    RightAlt = 0xA5,
    Semicolon = 0xBA,
    Plus = 0xBB,
    Comma = 0xBC,
    Minus = 0xBD,
    Period = 0xBE,
    Slash = 0xBF,
    Tilde = 0xC0,
    OpenBracket = 0xDB,
    Backslash = 0xDC,
    CloseBracket = 0xDD,
    Apostrophe = 0xDE,
};

enum class MouseButton : u8
{
    Left,
    Right,
    Middle,
    X1,
    X2,
};

enum class MouseAxis : u8
{
    X,
    Y,
    WheelX,
    WheelY,
};

enum class GamepadButton : u8
{
    A,
    B,
    X,
    Y,
    LeftBumper,
    RightBumper,
    Back,
    Start,
    LeftThumb,
    RightThumb,
    DPadUp,
    DPadDown,
    DPadLeft,
    DPadRight,
};

enum class GamepadAxis : u8
{
    LeftStickX,
    LeftStickY,
    RightStickX,
    RightStickY,
    LeftTrigger,
    RightTrigger,
};

// Trigger ////////////////////////////////////////////////////////////////////////////////////////

enum class Trigger : u8
{
    Pressed,
    Released,
};

// Modifier Flags /////////////////////////////////////////////////////////////////////////////////

enum class Modifiers : u8
{
    None = 0,
    Ctrl = 1 << 0,
    Shift = 1 << 1,
    Alt = 1 << 2,
};

}  // namespace Rndr

OPAL_ENUM_CLASS_FLAGS(Rndr::Modifiers);

namespace Rndr
{

// Callback Types /////////////////////////////////////////////////////////////////////////////////

using ButtonCallback = std::function<void(Trigger trigger, bool is_repeat)>;
using MouseButtonCallback = std::function<void(MouseButton button, Trigger trigger, const Vector2i& cursor_position)>;
using MousePositionCallback = std::function<void(MouseAxis axis, f32 delta)>;
using MouseWheelCallback = std::function<void(f32 delta_x, f32 delta_y)>;
using GamepadButtonCallback = std::function<void(GamepadButton button, Trigger trigger, u8 gamepad_index)>;
using GamepadAxisCallback = std::function<void(GamepadAxis axis, f32 value, u8 gamepad_index)>;
using TextCallback = std::function<void(uchar32 character)>;

// InputAction ////////////////////////////////////////////////////////////////////////////////////

/**
 * Represents a named input action that groups related bindings.
 *
 * Actions are created via the fluent builder returned by InputContext::AddAction(). Each action
 * has exactly one callback (set via the builder's On*() method) and one or more bindings.
 */
class InputAction
{
public:
    InputAction() = default;

    InputAction(const InputAction&) = delete;
    InputAction& operator=(const InputAction&) = delete;

    InputAction(InputAction&& other) noexcept;
    InputAction& operator=(InputAction&& other) noexcept;

    [[nodiscard]] const Opal::StringUtf8& GetName() const;
    [[nodiscard]] const GenericWindow* GetWindow() const;
    void SetWindow(const GenericWindow* window);

private:
    friend class InputActionBuilder;
    friend class InputContext;
    friend class InputSystem;

    enum class CallbackType : u8
    {
        None,
        Button,
        MouseButton,
        MousePosition,
        MouseWheel,
        GamepadButton,
        GamepadAxis,
        Text,
    };

    enum class BindingType : u8
    {
        Key,
        MouseButton,
        MouseAxis,
        GamepadButton,
        GamepadAxis,
        Hold,
        Combo,
        Text,
    };

    struct KeyBinding
    {
        Key key;
        Trigger trigger;
        Modifiers modifiers = Modifiers::None;
    };

    struct MouseButtonBinding
    {
        MouseButton button;
        Trigger trigger;
    };

    struct MouseAxisBinding
    {
        MouseAxis axis;
    };

    struct GamepadButtonBinding
    {
        GamepadButton button;
        Trigger trigger;
        u8 gamepad_index = 0;
    };

    struct GamepadAxisBinding
    {
        GamepadAxis axis;
        f32 dead_zone = -1.0f;
        u8 gamepad_index = 0;
    };

    struct HoldBinding
    {
        Key key;
        f32 duration_seconds;
        f32 elapsed_seconds = 0.0f;
        bool is_held = false;
        bool has_fired = false;
    };

    struct ComboBinding
    {
        Opal::DynamicArray<Key> keys;
        f32 timeout_seconds;
        i32 current_step = 0;
        f32 elapsed_since_last_step = 0.0f;
    };

    struct Binding
    {
        BindingType type;
        union
        {
            KeyBinding key;
            MouseButtonBinding mouse_button;
            MouseAxisBinding mouse_axis;
            GamepadButtonBinding gamepad_button;
            GamepadAxisBinding gamepad_axis;
            HoldBinding hold;
            ComboBinding combo;
        };

        Binding();
        Binding(const Binding& other);
        Binding& operator=(const Binding& other);
        Binding(Binding&& other) noexcept;
        Binding& operator=(Binding&& other) noexcept;
        ~Binding();
    };

    Opal::StringUtf8 m_name;
    const GenericWindow* m_window = nullptr;
    CallbackType m_callback_type = CallbackType::None;
    Opal::DynamicArray<Binding> m_bindings;

    ButtonCallback m_button_callback;
    MouseButtonCallback m_mouse_button_callback;
    MousePositionCallback m_mouse_position_callback;
    MouseWheelCallback m_mouse_wheel_callback;
    GamepadButtonCallback m_gamepad_button_callback;
    GamepadAxisCallback m_gamepad_axis_callback;
    TextCallback m_text_callback;
};

// InputActionBuilder /////////////////////////////////////////////////////////////////////////////

/**
 * Fluent builder for configuring an InputAction.
 *
 * Returned by InputContext::AddAction(). Allows chaining callback and binding configuration:
 *   context.AddAction("MoveForward")
 *       .OnButton(callback)
 *       .Bind(Key::W, Trigger::Pressed)
 *       .Bind(Key::Up, Trigger::Pressed);
 */
class InputActionBuilder
{
public:
    InputActionBuilder(InputAction& action);

    // Callback setters. Only one callback type per action.
    InputActionBuilder& OnButton(ButtonCallback callback);
    InputActionBuilder& OnMouseButton(MouseButtonCallback callback);
    InputActionBuilder& OnMousePosition(MousePositionCallback callback);
    InputActionBuilder& OnMouseWheel(MouseWheelCallback callback);
    InputActionBuilder& OnGamepadButton(GamepadButtonCallback callback);
    InputActionBuilder& OnGamepadAxis(GamepadAxisCallback callback);
    InputActionBuilder& OnText(TextCallback callback);

    // Key binding.
    InputActionBuilder& Bind(Key key, Trigger trigger, Modifiers modifiers = Modifiers::None);

    // Mouse button binding.
    InputActionBuilder& Bind(MouseButton button, Trigger trigger);

    // Mouse axis binding.
    InputActionBuilder& Bind(MouseAxis axis);

    // Gamepad button binding.
    InputActionBuilder& Bind(GamepadButton button, Trigger trigger, u8 gamepad_index = 0);

    // Gamepad axis binding.
    InputActionBuilder& Bind(GamepadAxis axis, f32 dead_zone = -1.0f, u8 gamepad_index = 0);

    // Hold binding. Fires a single callback after key is held for duration_seconds.
    InputActionBuilder& BindHold(Key key, f32 duration_seconds);

    // Combo binding. If timeout_seconds > 0, sequential combo. If timeout_seconds == 0, simultaneous chord.
    InputActionBuilder& BindCombo(Opal::ArrayView<Key> keys, f32 timeout_seconds);

    // Set the window filter for this action.
    InputActionBuilder& ForWindow(const GenericWindow* window);

private:
    Opal::Ref<InputAction> m_action;
};

// InputContext ////////////////////////////////////////////////////////////////////////////////////

/**
 * Represents a collection of named input actions.
 *
 * Contexts are pushed/popped on the InputSystem stack. The top context gets first chance to
 * handle events and can block propagation to lower contexts.
 */
class InputContext
{
public:
    InputContext() = default;
    explicit InputContext(Opal::StringUtf8 name);
    ~InputContext();

    InputContext(const InputContext&) = delete;
    InputContext& operator=(const InputContext&) = delete;

    InputContext(InputContext&& other) noexcept;
    InputContext& operator=(InputContext&& other) noexcept;

    [[nodiscard]] const Opal::StringUtf8& GetName() const;

    /**
     * Adds a new action and returns a builder for configuring it.
     * @param name Unique name for the action within this context.
     * @return Builder for chaining callback and binding configuration.
     */
    InputActionBuilder AddAction(const Opal::StringUtf8& name);

    /**
     * Removes an action by name.
     * @param name The action name to remove.
     * @return True if the action was found and removed.
     */
    bool RemoveAction(const Opal::StringUtf8& name);

    [[nodiscard]] bool ContainsAction(const Opal::StringUtf8& name) const;

    [[nodiscard]] InputAction* GetAction(const Opal::StringUtf8& name);
    [[nodiscard]] const InputAction* GetAction(const Opal::StringUtf8& name) const;

    [[nodiscard]] bool IsEnabled() const;
    void SetEnabled(bool enabled);

private:
    friend class InputSystem;

    Opal::StringUtf8 m_name;
    Opal::DynamicArray<InputAction> m_actions;
    bool m_enabled = true;
};

// InputSystem ////////////////////////////////////////////////////////////////////////////////////

/**
 * Manages input event processing via a stack of InputContexts.
 *
 * Owned by Application (no global singletons). Events arrive through the SystemMessageHandler
 * interface and are dispatched immediately to the matching actions in the context stack.
 * Contexts are processed top-to-bottom; the first context that handles an event blocks
 * propagation to lower contexts.
 *
 * Supports keyboard, mouse, gamepad (up to 4 via XInput), hold events, and combo sequences.
 * The system is not thread-safe.
 */
class InputSystem : public SystemMessageHandler
{
public:
    InputSystem();
    ~InputSystem() override;

    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    /**
     * Sets the default dead zone for analog inputs.
     * @param dead_zone Dead zone value in [0, 1] range.
     */
    void SetDefaultDeadZone(f32 dead_zone);
    [[nodiscard]] f32 GetDefaultDeadZone() const;

    /**
     * Returns the currently active input context (top of stack).
     */
    [[nodiscard]] InputContext& GetCurrentContext();
    [[nodiscard]] const InputContext& GetCurrentContext() const;

    /**
     * Pushes a context to the top of the stack.
     * @param context Reference to the context. The InputSystem does not manage the context lifetime.
     */
    void PushContext(InputContext& context);

    /**
     * Pops the top context from the stack.
     * @return True if a context was popped. False if only the default context remains.
     */
    bool PopContext();

    // SystemMessageHandler overrides.
    bool OnWindowClose(GenericWindow&) override;
    void OnWindowSizeChanged(const GenericWindow&, i32, i32) override;
    bool OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnCharacter(const GenericWindow& window, uchar32 character, bool is_repeated) override;
    bool OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position) override;
    bool OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y) override;

private:
    InputContext m_default_context;
    Opal::DynamicArray<Opal::Ref<InputContext>> m_contexts;
    f32 m_default_dead_zone = 0.2f;
};

}  // namespace Rndr

#endif  // RNDR_OLD_INPUT_SYSTEM
