#pragma once

namespace rndr
{

/**
 * Input primitives we can detect on different input devices.
 */
enum class InputPrimitive
{
    _KeyboardStart,
    Keyboard_W,
    Keyboard_A,
    Keyboard_S,
    Keyboard_D,
    Keyboard_Q,
    Keyboard_E,
    Keyboard_Space,
    Keyboard_Esc,
    _KeyboardEnd,

    _MouseStart,
    Mouse_LeftButton,
    Mouse_RightButton,
    Mouse_AxisX,
    Mouse_AxisY,
    Mouse_Position,
    _MouseEnd,

    Count
};

enum class InputTrigger
{
    Started,
    Finished,
    Triggered
};

}