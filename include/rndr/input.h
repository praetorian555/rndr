#pragma once

#include <functional>

#include "opal/container/scope-ptr.h"
#include "opal/container/array-view.h"
#include "opal/container/string.h"

#include "rndr/input-primitives.h"
#include "rndr/math.h"
#include "rndr/platform/windows-forward-def.h"

namespace Rndr
{

/**
 * User defined input action.
 */
struct InputAction
{
public:
    InputAction() = default;
    explicit InputAction(Opal::StringUtf8 name);

    [[nodiscard]] const Opal::StringUtf8& GetName() const;
    [[nodiscard]] bool IsValid() const;

    bool operator==(const InputAction& other) const = default;

private:
    Opal::StringUtf8 m_name;
};

/**
 * Represents an input event that can be bound to an input action.
 */
struct InputBinding
{
    InputPrimitive primitive;
    InputTrigger trigger;
    float modifier = 1.0f;

    /**
     * Compares two input bindings. The modifier is not considered.
     * @param other The other input binding to compare.
     * @return Returns true if the input bindings are equal, false otherwise.
     */
    bool operator==(const InputBinding& other) const;
};

/**
 * Represents a callback that is invoked when an input action is triggered.
 */
using InputCallback = std::function<void(InputPrimitive primitive, InputTrigger trigger, float value)>;

/**
 * Helper struct to initialize an input action in the input context.
 */
struct InputActionData
{
    InputCallback callback = nullptr;
    /** If null, callback will be called for any window triggering one of the bindings. */
    NativeWindowHandle native_window = nullptr;
    Opal::ArrayView<InputBinding> bindings;
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
    InputContext();

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

    /**
     * Adds an input action to the input context.
     * @param action The input action to add.
     * @param data The data associated with the input action. The callback can't be null.
     * @return Returns true if the input action was successfully added.
     */
    bool AddAction(const InputAction& action, const InputActionData& data);

    /**
     * Add an input binding to an input action.
     * @param action Action to add the binding to.
     * @param binding Binding to add to the action.
     * @return Returns true if the binding was successfully added.
     */
    bool AddBindingToAction(const InputAction& action, const InputBinding& binding);

    /**
     * Removes an input action from the input context.
     * @param action The input action to remove.
     * @return Returns true if the input action was successfully removed.
     */
    bool RemoveBindingFromAction(const InputAction& action, const InputBinding& binding);

    /**
     * Checks if the input context contains an input action.
     * @param action The input action to check.
     * @return Returns true if the input context contains the input action.
     */
    [[nodiscard]] bool ContainsAction(const InputAction& action) const;

    /**
     * Returns the callback associated with an input action.
     * @param action The input action to get the callback for.
     * @return Returns the callback associated with the input action.
     */
    [[nodiscard]] InputCallback GetActionCallback(const InputAction& action) const;

    /**
     * Returns the input bindings associated with an input action.
     * @param action The input action to get the bindings for.
     * @return Returns the input bindings associated with the input action.
     */
    [[nodiscard]] Opal::ArrayView<InputBinding> GetActionBindings(const InputAction& action) const;

private:
    Opal::ScopePtr<struct InputContextData> m_context_data;
    Opal::StringUtf8 m_name;

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
class InputSystem
{
public:
    /**
     * Initializes the input system.
     * @return Returns true if the input system was successfully initialized.
     */
    static bool Init();

    /**
     * Destroys the input system.
     * @return Returns true if the input system was successfully destroyed.
     */
    static bool Destroy();

    /**
     * Returns the currently active input context.
     * @return Returns the input context on top of the stack.
     */
    static InputContext& GetCurrentContext();

    /**
     * Pushes a new input context to the top of the stack making it the active context.
     * @param context Context to be pushed to the top of the stack.
     * @note The input context lifetime is not managed by the input system.
     */
    static bool PushContext(const InputContext& context);

    /**
     * Pops the input context from the top of the stack. This does not destroy the context.
     */
    static bool PopContext();

    /**
     * Processes all pending input events.
     * @param delta_seconds Time elapsed since the last frame.
     */
    static bool ProcessEvents(float delta_seconds);

    /**
     * Submits a button event to the input system.
     * @param window Native window handle in which the event was detected.
     * @param primitive Input primitive that was triggered.
     * @param trigger Input trigger detected. For buttons this is InputTrigger::ButtonDown,
     * InputTrigger::ButtonUp or InputTrigger::DoubleClick.
     */
    static bool SubmitButtonEvent(NativeWindowHandle window, InputPrimitive primitive, InputTrigger trigger);

    /**
     * Submits a mouse position event to the input system.
     * @param window Native window handle in which the event was detected.
     * @param position Mouse position in screen coordinates, where the origin is in the bottom left
     * corner.
     * @param screen_size Size of the screen in pixels.
     */
    static bool SubmitMousePositionEvent(NativeWindowHandle window, const Point2f& position, const Vector2f& screen_size);

    /**
     * Submits a relative mouse position event to the input system. Used in case of infinite cursor
     * mode.
     * @param window Native window handle in which the event was detected.
     * @param delta_position Relative mouse position in screen coordinates, relative to the last
     * mouse position. Positive values mean the mouse moved to the right or up.
     * @param screen_size Size of the screen in pixels.
     */
    static bool SubmitRelativeMousePositionEvent(NativeWindowHandle window, const Vector2f& delta_position, const Vector2f& screen_size);

    /**
     * Submits a mouse wheel event to the input system.
     * @param window Native window handle in which the event was detected.
     * @param delta_wheel Number of ticks the mouse wheel was rotated. Positive values mean the
     * wheel was rotated forward, away from the user.
     */
    static bool SubmitMouseWheelEvent(NativeWindowHandle window, int delta_wheel);

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

    /**
     * Helper function to check if the given binding has a valid combination of primitives and
     * triggers.
     * @param binding Input binding to check.
     * @return Returns true if the binding is valid.
     */
    static bool IsBindingValid(const InputBinding& binding);

private:
    static Opal::ScopePtr<struct InputSystemData> g_system_data;
};

/**
 * Helper macro to bind a member function to an input callback.
 */
#define RNDR_BIND_INPUT_CALLBACK(this, func_ptr)                                                                           \
    std::bind(&std::remove_reference<decltype(*this)>::type::func_ptr, this, std::placeholders::_1, std::placeholders::_2, \
              std::placeholders::_3)

}  // namespace Rndr
