#pragma once

#include <functional>

#include "math/point2.h"
#include "math/vector2.h"

#include "rndr/core/base.h"
#include "rndr/core/inputprimitives.h"
#include "rndr/core/span.h"
#include "rndr/core/string.h"

namespace rndr
{

/**
 * User defined input action.
 */
struct InputAction
{
public:
    InputAction() = default;
    explicit InputAction(String name);

    [[nodiscard]] const String& GetName() const;
    [[nodiscard]] bool IsValid() const;

    bool operator==(const InputAction& other) const;
    bool operator!=(const InputAction& other) const;

private:
    String m_name;
};

/**
 * Represents an input event that can be bound to an input action.
 */
struct InputBinding
{
    InputPrimitive primitive;
    InputTrigger trigger;
    real modifier = MATH_REALC(1.0);

    /**
     * Compares two input bindings. The modifier is not considered.
     * @param other The other input binding to compare.
     * @return Returns true if the input bindings are equal, false otherwise.
     */
    bool operator==(const InputBinding& other) const;

    /**
     * Compares two input bindings. The modifier is not considered.
     * @param other The other input binding to compare.
     * @return Returns true if the input bindings are not equal, false otherwise.
     */
    bool operator!=(const InputBinding& other) const;
};

/**
 * Represents a callback that is invoked when an input action is triggered.
 */
using InputCallback =
    std::function<void(InputPrimitive primitive, InputTrigger trigger, real value)>;

/**
 * Helper struct to initialize an input action in the input context.
 */
struct InputActionData
{
    InputCallback callback = nullptr;
    /** If null, callback will be called for any window triggering one of the bindings. */
    OpaquePtr native_window = nullptr;
    Span<InputBinding> bindings;
};

// Forward declaration.
class InputContext;
namespace InputSystem
{
bool Init();
InputContext& GetCurrentContext();
bool PushContext(const InputContext& context);
}  // namespace InputSystem

/**
 * Represents a collection of mappings between input actions and input bindings and callbacks.
 */
class InputContext
{
public:
    /**
     * Creates a new input context.
     * @param name Name of the input context.
     */
    explicit InputContext(String name);

    /**
     * Destroys the input context.
     */
    ~InputContext();

    /**
     * Create a copy of the input context.
     * @param other The input context to copy.
     */
    InputContext(const InputContext& other);

    /**
     * Assigns the input context to another input context.
     * @param other The input context to assign.
     * @return Returns a reference to the input context.
     */
    InputContext& operator=(const InputContext& other);

    /**
     * Move constructs the input context.
     * @param other The input context to move.
     */
    InputContext(InputContext&& other) noexcept;

    /**
     * Move assigns the input context.
     * @param other The input context to move.
     * @return Returns a reference to the input context.
     */
    InputContext& operator=(InputContext&& other) noexcept;

    /**
     * Returns the name of the input context.
     * @return Returns the name of the input context.
     */
    [[nodiscard]] const String& GetName() const;

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
    [[nodiscard]] Span<InputBinding> GetActionBindings(const InputAction& action) const;

private:
    OpaquePtr m_context_data = nullptr;
    String m_name;

    // Implementation details. ////////////////////////////////////////////////////////////////////
    friend bool InputSystem::Init();
    friend InputContext& InputSystem::GetCurrentContext();
    friend bool InputSystem::PushContext(const InputContext& context);
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
// TODO(Marko): Provide testing of input bindings to make sure they make sense.
namespace InputSystem
{
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

/**
 * Returns the currently active input context.
 * @return Returns the input context on top of the stack.
 */
InputContext& GetCurrentContext();

/**
 * Pushes a new input context to the top of the stack making it the active context.
 * @param context Context to be pushed to the top of the stack.
 * @note The input context lifetime is not managed by the input system.
 */
bool PushContext(const InputContext& context);

/**
 * Pops the input context from the top of the stack. This does not destroy the context.
 */
bool PopContext();

/**
 * Processes all pending input events.
 * @param delta_seconds Time elapsed since the last frame.
 */
bool ProcessEvents(real delta_seconds);

/**
 * Submits a button event to the input system.
 * @param window Native window handle in which the event was detected.
 * @param primitive Input primitive that was triggered.
 * @param trigger Input trigger detected. For buttons this is InputTrigger::ButtonDown,
 * InputTrigger::ButtonUp or InputTrigger::DoubleClick.
 */
bool SubmitButtonEvent(OpaquePtr window, InputPrimitive primitive, InputTrigger trigger);

/**
 * Submits a mouse position event to the input system.
 * @param window Native window handle in which the event was detected.
 * @param position Mouse position in screen coordinates, where the origin is in the bottom left
 * corner.
 * @param screen_size Size of the screen in pixels.
 */
bool SubmitMousePositionEvent(OpaquePtr window,
                              const math::Point2& position,
                              const math::Vector2& screen_size);

/**
 * Submits a relative mouse position event to the input system. Used in case of infinite cursor
 * mode.
 * @param window Native window handle in which the event was detected.
 * @param delta_position Relative mouse position in screen coordinates, relative to the last mouse
 * position. Positive values mean the mouse moved to the right or up.
 * @param screen_size Size of the screen in pixels.
 */
bool SubmitRelativeMousePositionEvent(OpaquePtr window,
                                      const math::Vector2& delta_position,
                                      const math::Vector2& screen_size);

/**
 * Submits a mouse wheel event to the input system.
 * @param window Native window handle in which the event was detected.
 * @param delta_wheel Number of ticks the mouse wheel was rotated. Positive values mean the wheel
 * was rotated forward, away from the user.
 */
bool SubmitMouseWheelEvent(OpaquePtr window, int delta_wheel);

/**
 * Helper function to check if the primitive is a button.
 * @param primitive Input primitive to check.
 * @return Returns true if the primitive is a button.
 */
bool IsButton(InputPrimitive primitive);

/**
 * Helper function to check if the primitive is a mouse button.
 * @param primitive Input primitive to check.
 * @return Returns true if the primitive is a mouse button.
 */
bool IsMouseButton(InputPrimitive primitive);

/**
 * Helper function to check if the primitive is a keyboard button.
 * @param primitive Input primitive to check.
 * @return Returns true if the primitive is a keyboard button.
 */
bool IsKeyboardButton(InputPrimitive primitive);

/**
 * Helper function to check if the primitive is an axis.
 * @param primitive Input primitive to check.
 * @return Returns true if the primitive is an axis.
 */
bool IsAxis(InputPrimitive primitive);

}  // namespace InputSystem

/**
 * Helper macro to bind a member function to an input callback.
 */
#define RNDR_BIND_INPUT_CALLBACK(this, func_ptr)                       \
    std::bind(&std::remove_reference<decltype(*this)>::type::func_ptr, \
              this,                                                    \
              std::placeholders::_1,                                   \
              std::placeholders::_2,                                   \
              std::placeholders::_3)

}  // namespace rndr
