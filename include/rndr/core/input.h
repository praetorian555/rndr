#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rndr/core/base.h"
#include "rndr/core/inputprimitives.h"
#include "rndr/core/window.h"

namespace rndr
{

/**
 * User defined input action.
 */
using InputAction = std::string;

/**
 *
 */
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

    InputMapping(const InputAction& Action, InputCallback Callback)
        : Action(Action), Callback(Callback)
    {
    }
};

/**
 * Represents a collection of input mappings that can be used to handle input events.
 */
struct InputContext
{
    std::map<InputAction, std::unique_ptr<InputMapping>> Mappings;

    InputMapping* CreateMapping(const InputAction& Action, InputCallback Callback);
    void AddBinding(const InputAction& Action,
                    InputPrimitive Primitive,
                    InputTrigger Trigger,
                    real Modifier = 1.0);

    const InputMapping* GetMapping(const InputAction& Action);
};

class InputSystem
{
public:
    static InputSystem* Get();

    void Init();
    void ShutDown();

    void Update(real DeltaSeconds);

    void SetWindow(const Window* Window);

    InputContext* GetContext();

    bool IsButton(InputPrimitive Primitive) const;
    bool IsMouseButton(InputPrimitive Primitive) const;
    bool IsKeyboardButton(InputPrimitive Primitive) const;

    bool IsAxis(InputPrimitive Primitive) const;
    bool IsMouseAxis(InputPrimitive Primitive) const;

    rndr::Point2i GetMousePosition() const;

private:
    static std::unique_ptr<InputSystem> s_Input;

    InputContext* m_Context;
    int m_X = 0, m_Y = 0;
    bool m_FirstTime = true;

    const rndr::Window* m_Window = nullptr;
};

#define RNDR_BIND_INPUT_CALLBACK(FuncPtr, This) RNDR_BIND_THREE_PARAM(This, FuncPtr)

}  // namespace rndr