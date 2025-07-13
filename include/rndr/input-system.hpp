#pragma once

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
