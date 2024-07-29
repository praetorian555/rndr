#pragma once

#include "rndr/core/delegate.h"
#include "rndr/core/input.h"
#include "rndr/core/math.h"
#include "rndr/core/projection-camera.h"
#include "rndr/core/types.h"

namespace Rndr
{

class Window;
enum class CursorMode;

struct FlyCameraDesc
{
    Point3f start_position = Point3f::Zero();
    Rotatorf start_rotation = Rotatorf::Zero();
    f32 movement_speed = 1.0f;
    f32 rotation_speed = 8000.0f;
    ProjectionCameraDesc projection_desc;
};

/**
 * Represents camera in the world space. It can be moved using W, A, S and D keys and can look
 * around using the mouse.
 */
class FlyCamera : public ProjectionCamera
{
public:
    explicit FlyCamera(Window* window, InputContext* input_context, const FlyCameraDesc& desc = FlyCameraDesc{});
    ~FlyCamera();

    void Update(f32 delta_seconds);

private:
    // Private methods

    void HandleActivate(InputPrimitive primitive, InputTrigger trigger, f32 value);
    void HandleLookVert(InputPrimitive primitive, InputTrigger trigger, f32 axis_value);
    void HandleLookHorz(InputPrimitive primitive, InputTrigger trigger, f32 axis_value);
    void HandleMoveForward(InputPrimitive primitive, InputTrigger trigger, f32 value);
    void HandleMoveRight(InputPrimitive primitive, InputTrigger trigger, f32 value);

    // Private fields

    Window* m_window = nullptr;
    InputContext* m_input_context = nullptr;
    FlyCameraDesc m_desc;

    Rotatorf m_delta_rotation = Rotatorf::Zero();
    Vector3f m_delta_position = Vector3f::Zero();

    Vector3f m_direction_vector = Vector3f::Zero();
    Vector3f m_right_vector = Vector3f::Zero();
    DelegateHandle m_window_resize_handle;
    CursorMode m_prev_cursor_mode;

    bool m_is_active = false;
};

}  // namespace Rndr
