#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rndr/core/inputprimitives.h"
#include "rndr/core/rndr.h"

namespace rndr
{

/**
 * User defined name of the action.
 */
using InputAction = std::string;

struct InputBinding
{
    InputPrimitive Primitive;
    InputTrigger Trigger;
};

using InputCallback = std::function<void(InputBinding, int X, int Y)>;

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

struct InputContext
{
    std::map<InputAction, std::unique_ptr<InputMapping>> Mappings;

    InputMapping* CreateMapping(const InputAction& Action, InputCallback Callback);
    void AddBinding(const InputAction& Action, InputPrimitive Primitive, InputTrigger Trigger);

    const InputMapping* GetMapping(const InputAction& Action);
};

class InputSystem
{
public:
    static InputSystem* Get();

    void Init();
    void ShutDown();

    void Update(real DeltaSeconds);

    void SetContext(const InputContext* Context);

private:
    static std::unique_ptr<InputSystem> s_Input;

    const InputContext* m_Context;
    int m_X = 0, m_Y = 0;
    bool m_FirstTime = true;
};

}  // namespace rndr