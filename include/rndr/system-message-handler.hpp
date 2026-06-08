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
};
}  // namespace Rndr