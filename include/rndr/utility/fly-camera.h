#pragma once

#include "rndr/core/base.h"
#include "rndr/core/delegate.h"
#include "rndr/core/input.h"
#include "rndr/core/projection-camera.h"

namespace Rndr
{

class Window;
enum class CursorMode;

struct FlyCameraDesc
{
    math::Point3 start_position;
    math::Rotator start_rotation;
    float movement_speed = 1.0f;
    float rotation_speed = 8000.0f;
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

    void Update(real delta_seconds);

private:
    // Private methods

    void HandleLookVert(InputPrimitive primitive, InputTrigger trigger, real axis_value);
    void HandleLookHorz(InputPrimitive primitive, InputTrigger trigger, real axis_value);
    void HandleMoveForward(InputPrimitive primitive, InputTrigger trigger, real value);
    void HandleMoveRight(InputPrimitive primitive, InputTrigger trigger, real value);

    // Private fields

    Window* m_window = nullptr;
    InputContext* m_input_context = nullptr;
    FlyCameraDesc m_desc;

    math::Rotator m_delta_rotation;
    math::Vector3 m_delta_position;

    math::Vector3 m_direction_vector;
    math::Vector3 m_right_vector;
    DelegateHandle m_window_resize_handle;
    CursorMode m_prev_cursor_mode;
};

}  // namespace Rndr