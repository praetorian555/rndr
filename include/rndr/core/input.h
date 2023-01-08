#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <variant>
#include <vector>

#include "math/point2.h"
#include "math/vector2.h"

#include "rndr/core/base.h"
#include "rndr/core/inputprimitives.h"

namespace rndr
{

/**
 * User defined input action.
 */
using InputAction = std::string;

struct InputBinding
{
    InputPrimitive Primitive;
    InputTrigger Trigger;

    real Modifier = 1.0;
};

using InputCallback =
    std::function<void(InputPrimitive Primitive, InputTrigger Trigger, real Value)>;

/**
 * Maps a user-defined action to the array of input bindings and callback function.
 */
struct InputMapping
{
    InputAction Action;
    std::vector<InputBinding> Bindings;
    InputCallback Callback;

    InputMapping(InputAction Action, InputCallback Callback)
        : Action(std::move(Action)), Callback(std::move(Callback))
    {
    }
};

/**
 * Represents a collection of input mappings that can be used to handle input events.
 */
struct InputContext
{
    struct Entry
    {
        InputAction Action;
        std::unique_ptr<InputMapping> Mapping;
    };

    std::vector<Entry> Mappings;

    InputMapping* CreateMapping(const InputAction& Action, const InputCallback& Callback);
    void AddBinding(const InputAction& Action,
                    InputPrimitive Primitive,
                    InputTrigger Trigger,
                    real Modifier = 1.0);

    const InputMapping* GetMapping(const InputAction& Action);
};

class InputSystem
{
public:
    InputSystem();
    ~InputSystem();

    InputSystem(const InputSystem& Other) = delete;
    InputSystem& operator=(const InputSystem& Other) = delete;
    
    InputSystem(InputSystem&& Other) = delete;
    InputSystem& operator=(InputSystem&& Other) = delete;

    void SubmitButtonEvent(NativeWindowHandle Window,
                           InputPrimitive Primitive,
                           InputTrigger Trigger);
    void SubmitMousePositionEvent(NativeWindowHandle Window,
                                  const math::Point2& Position,
                                  const math::Vector2& ScreenSize);
    void SubmitMouseWheelEvent(NativeWindowHandle Window, int DeltaWheel);

    void Update(real DeltaSeconds);

    InputContext* GetContext();

    static bool IsButton(InputPrimitive Primitive);
    static bool IsMouseButton(InputPrimitive Primitive);
    static bool IsKeyboardButton(InputPrimitive Primitive);

    static bool IsAxis(InputPrimitive Primitive);
    static bool IsMouseAxis(InputPrimitive Primitive);

    [[nodiscard]] math::Point2 GetMousePosition() const;

private:

    // Private types

    struct ButtonEvent
    {
        rndr::InputPrimitive Primitive;
        rndr::InputTrigger Trigger;
    };

    struct MousePositionEvent
    {
        math::Point2 Position;
        math::Vector2 ScreenSize;
    };

    struct MouseWheelEvent
    {
        int DeltaWheel = 0;
    };

    struct Event
    {
        NativeWindowHandle WindowHandle;
        std::variant<ButtonEvent, MousePositionEvent, MouseWheelEvent> Data;
    };

    // Private methods

    void ProcessEvent(const ButtonEvent& Event);
    void ProcessEvent(const MousePositionEvent& Event);
    void ProcessEvent(const MouseWheelEvent& Event);

    // Private fields

    InputContext* m_Context = nullptr;
    std::optional<math::Point2> m_AbsolutePosition;

    std::queue<Event> m_Events;
};

#define RNDR_BIND_INPUT_CALLBACK(FuncPtr, This) RNDR_BIND_THREE_PARAM(This, FuncPtr)

}  // namespace rndr