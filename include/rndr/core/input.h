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

    bool operator==(const InputBinding& other) const;
    bool operator!=(const InputBinding& other) const;
};

/**
 * Represents a callback that is invoked when an input action is triggered.
 */
using InputCallback =
    std::function<void(InputPrimitive primitive, InputTrigger trigger, real value)>;

/**
 * Represents a collection of mappings between input actions and input bindings and callbacks.
 */
class InputContext
{
public:
    InputContext();
    ~InputContext();

    InputContext(const InputContext&);
    InputContext& operator=(const InputContext&);

    InputContext(InputContext&&) noexcept;
    InputContext& operator=(InputContext&&) noexcept;

    bool AddAction(const InputAction& action,
                   const InputCallback& callback,
                   const Span<InputBinding>& bindings);

    bool AddBindingToAction(const InputAction& action, const InputBinding& binding);
    bool RemoveBindingFromAction(const InputAction& action, const InputBinding& binding);

    [[nodiscard]] bool ContainsAction(const InputAction& action) const;
    [[nodiscard]] InputCallback GetActionCallback(const InputAction& action) const;
    [[nodiscard]] Span<InputBinding> GetActionBindings(const InputAction& action) const;

private:
    OpaquePtr m_context_data = nullptr;
};

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
void PushContext(const InputContext& context);

/**
 * Pops the input context from the top of the stack. This does not destroy the context.
 */
void PopContext();

/**
 * Processes all pending input events.
 * @param delta_seconds Time elapsed since the last frame.
 */
void ProcessEvents(real delta_seconds);

/**
 * Submits a button event to the input system.
 * @param window Native window handle in which the event was detected.
 * @param primitive Input primitive that was triggered.
 * @param trigger Input trigger detected. For buttons this is InputTrigger::ButtonDown,
 * InputTrigger::ButtonUp or InputTrigger::DoubleClick.
 */
void SubmitButtonEvent(OpaquePtr window, InputPrimitive primitive, InputTrigger trigger);

/**
 * Submits a mouse position event to the input system.
 * @param window Native window handle in which the event was detected.
 * @param position Mouse position in screen coordinates, where the origin is in the bottom left
 * corner.
 * @param screen_size Size of the screen in pixels.
 */
void SubmitMousePositionEvent(OpaquePtr window,
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
void SubmitRelativeMousePositionEvent(OpaquePtr window,
                                      const math::Vector2& delta_position,
                                      const math::Vector2& screen_size);

/**
 * Submits a mouse wheel event to the input system.
 * @param window Native window handle in which the event was detected.
 * @param delta_wheel Number of ticks the mouse wheel was rotated. Positive values mean the wheel
 * was rotated forward, away from the user.
 */
void SubmitMouseWheelEvent(OpaquePtr window, int delta_wheel);

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