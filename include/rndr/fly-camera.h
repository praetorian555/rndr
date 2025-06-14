#pragma once

#include "rndr/delegate.h"
#include "rndr/input.h"
#include "rndr/math.h"
#include "rndr/projection-camera.h"
#include "rndr/types.h"

namespace Rndr
{

class Window;
enum class CursorMode;

struct FlyCameraDesc
{
    Point3f start_position = Point3f::Zero();
    Point3f target_position = Point3f(0, 0, -100);
    ProjectionCameraDesc projection_desc;
};

/**
 * Represents camera in the world space. It can be moved using W, A, S and D keys and can look
 * around using the mouse.
 */
class FlyCamera : public ProjectionCamera
{
public:
    explicit FlyCamera(i32 screen_width, i32 screen_height, const FlyCameraDesc& desc = FlyCameraDesc{});

    void MoveForward(f32 amount_world_units);
    void MoveRight(f32 amount_world_units);
    void AddYaw(f32 yaw_radians);
    void AddRoll(f32 roll_radians);

private:
    FlyCameraDesc m_desc;
    f32 m_yaw_radians = 0;
    f32 m_roll_radians = 0;
    Quaternionf m_start_rotation = Quaternionf::Identity();
};

}  // namespace Rndr
