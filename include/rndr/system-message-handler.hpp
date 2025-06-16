#pragma once

#include "rndr/generic-window.hpp"
#include "rndr/input-primitives.h"
#include "rndr/math.h"
#include "rndr/types.h"

namespace Rndr
{
struct SystemMessageHandler
{
    virtual ~SystemMessageHandler() = default;

    virtual bool OnWindowClose(GenericWindow& window) = 0;
    virtual void OnWindowSizeChanged(const GenericWindow& window, i32 width, i32 height) = 0;

    virtual bool OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) = 0;
    virtual bool OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) = 0;
    virtual bool OnCharacter(const GenericWindow& window, uchar32 character, bool is_repeated) = 0;

    virtual bool OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) = 0;
    virtual bool OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) = 0;
    virtual bool OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) = 0;
    virtual bool OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position) = 0;
    virtual bool OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y) = 0;
};
}  // namespace Rndr