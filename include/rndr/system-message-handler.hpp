#pragma once

#include "rndr/input-primitives.hpp"
#include "rndr/math.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

class GenericWindow;

struct SystemMessageHandler
{
    virtual ~SystemMessageHandler() = default;

    virtual bool OnWindowClose(GenericWindow& window) = 0;
    virtual void OnWindowSizeChanged(const GenericWindow& window, i32 width, i32 height) = 0;
    virtual void OnMonitorChange() {}
    virtual void OnWindowDpiChanged(const GenericWindow& window, f32 new_dpi_scale) { (void)window; (void)new_dpi_scale; }

    virtual bool OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) = 0;
    virtual bool OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) = 0;
    virtual bool OnCharacter(const GenericWindow& window, uchar32 character, bool is_repeated) = 0;

    // Mouse callbacks below report `cursor_position` in client-space coordinates: pixels relative to
    // the upper-left corner of the window's client area, with +x going right and +y going down. This
    // is independent of where the window sits on the desktop, so (0, 0) is always the window's top-left.
    virtual bool OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) = 0;
    virtual bool OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) = 0;
    virtual bool OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) = 0;
    virtual bool OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position) = 0;

    // Reports mouse movement as both the relative motion since the last event (`delta_x`, `delta_y`)
    // and the resulting absolute `cursor_position` in client space (see the convention above).
    virtual bool OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y, const Vector2i& cursor_position) = 0;

    // Gamepad callbacks below carry no window. The platform gamepad API has no notion of which window
    // a pad belongs to, so gamepad events are global to the application.
    //
    // `gamepad_index` is the slot the pad is connected on, in [0, k_max_gamepads).
    virtual bool OnGamepadButtonDown(u8 gamepad_index, GamepadButton button)
    {
        (void)gamepad_index;
        (void)button;
        return false;
    }
    virtual bool OnGamepadButtonUp(u8 gamepad_index, GamepadButton button)
    {
        (void)gamepad_index;
        (void)button;
        return false;
    }

    // Reports a raw axis value with no dead zone applied: [-1, 1] for sticks, [0, 1] for triggers.
    // Dead zones belong to the individual bindings, so applying one here would filter twice.
    virtual bool OnGamepadAxis(u8 gamepad_index, GamepadAxis axis, f32 value)
    {
        (void)gamepad_index;
        (void)axis;
        (void)value;
        return false;
    }

    virtual void OnGamepadConnectionChanged(u8 gamepad_index, bool is_connected)
    {
        (void)gamepad_index;
        (void)is_connected;
    }
};
}  // namespace Rndr