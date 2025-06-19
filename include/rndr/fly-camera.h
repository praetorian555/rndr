#pragma once

#include "rndr/delegate.h"
#include "rndr/input-system.hpp"
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
    f32 start_yaw_radians = 0.0f;
    f32 start_pitch_radians = 0.0f;
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
    void AddPitch(f32 pitch_radians);

    void Tick(f32 delta_seconds);

private:
    static f32 ClampYaw(f32 yaw);
    static f32 ClampPitch(f32 pitch);

    FlyCameraDesc m_desc;
    f32 m_yaw_radians = 0;
    f32 m_pitch_radians = 0;
    f32 m_delta_yaw_radians = 0;
    f32 m_delta_pitch_radians = 0;
    f32 m_delta_move_forward = 0.0f;
    f32 m_delta_move_right = 0.0f;
    Quaternionf m_start_rotation = Quaternionf::Identity();
};

}  // namespace Rndr
