#pragma once

namespace rndr
{

/**
 * Input primitives we can detect on different input devices.
 */
enum class InputPrimitive
{
    _KeyboardStart,
    Keyboard_A,
    Keyboard_B,
    Keyboard_C,
    Keyboard_D,
    Keyboard_E,
    Keyboard_F,
    Keyboard_G,
    Keyboard_H,
    Keyboard_I,
    Keyboard_J,
    Keyboard_K,
    Keyboard_L,
    Keyboard_M,
    Keyboard_N,
    Keyboard_O,
    Keyboard_P,
    Keyboard_Q,
    Keyboard_R,
    Keyboard_S,
    Keyboard_T,
    Keyboard_U,
    Keyboard_V,
    Keyboard_W,
    Keyboard_X,
    Keyboard_Y,
    Keyboard_Z,
    Keyboard_0,
    Keyboard_1,
    Keyboard_2,
    Keyboard_3,
    Keyboard_4,
    Keyboard_5,
    Keyboard_6,
    Keyboard_7,
    Keyboard_8,
    Keyboard_9,
    Keyboard_F1,
    Keyboard_F2,
    Keyboard_F3,
    Keyboard_F4,
    Keyboard_F5,
    Keyboard_F6,
    Keyboard_F7,
    Keyboard_F8,
    Keyboard_F9,
    Keyboard_F10,
    Keyboard_F11,
    Keyboard_F12,
    Keyboard_LeftShift,
    Keyboard_RightShift,
    Keyboard_LeftAlt,
    Keyboard_RightAlt,
    Keyboard_LeftControl,
    Keyboard_RightControl,
    Keyboard_LeftArrow,
    Keyboard_RightArrow,
    Keyboard_UpArrow,
    Keyboard_DownArrow,
    Keyboard_Space,
    Keyboard_Esc,
    _KeyboardEnd,

    _MouseStart,
    Mouse_LeftButton,
    Mouse_RightButton,
    Mouse_MiddleButton,
    Mouse_AxisX,
    Mouse_AxisY,
    Mouse_AxisWheel,
    _MouseEnd,

    Count
};

enum class InputTrigger
{
    ButtonPressed,
    ButtonReleased,
    ButtonDoubleClick,
    AxisChangedRelative,
    AxisChangedAbsolute
};

}  // namespace rndr