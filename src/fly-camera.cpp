#include "rndr/fly-camera.h"

#include "opal/container/dynamic-array.h"
#include "opal/math/transform.h"

#include "rndr/types.h"
#include "rndr/window.h"

Rndr::FlyCamera::FlyCamera(i32 screen_width, i32 screen_height, const FlyCameraDesc& desc)
    : ProjectionCamera(desc.start_position, Quaternionf::Identity(),
                       screen_width, screen_height, desc.projection_desc),
      m_desc(desc)
{
    m_start_rotation = Quaternionf(Opal::LookAt_RH(desc.start_position, desc.target_position, Vector3f(0, 1, 0)));
    // SetRotation(m_start_rotation);
}

void Rndr::FlyCamera::MoveForward(f32 amount_world_units)
{
    m_delta_move_forward += amount_world_units;
}

void Rndr::FlyCamera::MoveRight(f32 amount_world_units)
{
    m_delta_move_right += amount_world_units;
}

void Rndr::FlyCamera::AddYaw(f32 yaw_radians)
{
    m_delta_yaw_radians += yaw_radians;
}

void Rndr::FlyCamera::AddPitch(f32 pitch_radians)
{
    m_delta_pitch_radians += pitch_radians;
}

void Rndr::FlyCamera::Tick(f32)
{
    m_yaw_radians += m_delta_yaw_radians;
    if (m_yaw_radians > Opal::k_pi_float)
    {
        m_yaw_radians -= 2 * Opal::k_pi_float;
    }
    if (m_yaw_radians < -Opal::k_pi_float)
    {
        m_yaw_radians += 2 * Opal::k_pi_float;
    }

    m_pitch_radians += m_delta_pitch_radians;
    f32 pitch_degrees = Opal::Degrees(m_pitch_radians);
    pitch_degrees = Opal::Clamp(pitch_degrees, -89.0f, 89.0f);
    m_pitch_radians = Opal::Radians(pitch_degrees);

    // Up is the same for both local camera coordinate system and in the world coordinate system
    constexpr Vector3f k_up = {0, 1, 0};
    constexpr Vector3f k_local_forward = {0, 0, -1};
    const Quaternionf yaw_quat = Quaternionf::FromAxisAngleRadians(k_up, m_yaw_radians);
    const Vector3f world_forward = Opal::Normalize(yaw_quat * k_local_forward);
    const Vector3f world_right = Opal::Normalize(Opal::Cross(world_forward, k_up));
    const Quaternionf pitch_quat = Quaternionf::FromAxisAngleRadians(world_right, m_pitch_radians);
    SetRotation(Normalize(pitch_quat * yaw_quat));

    const Vector3f total_move_vector = m_delta_move_forward * world_forward + m_delta_move_right * world_right;
    SetPosition(GetPosition() + total_move_vector);

    m_delta_yaw_radians = 0;
    m_delta_pitch_radians = 0;
    m_delta_move_forward = 0;
    m_delta_move_right = 0;
}
