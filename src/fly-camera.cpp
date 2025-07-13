#include "rndr/fly-camera.hpp"

#include "opal/math/transform.h"

#include "rndr/log.hpp"
#include "rndr/types.hpp"

Rndr::FlyCamera::FlyCamera(i32 screen_width, i32 screen_height, const FlyCameraDesc& desc)
    : ProjectionCamera(desc.start_position, Quaternionf::Identity(), screen_width, screen_height, desc.projection_desc),
      m_desc(desc),
      m_pitch_radians(desc.start_pitch_radians),
      m_yaw_radians(desc.start_yaw_radians)
{
    m_yaw_radians = ClampYaw(m_yaw_radians);
    m_pitch_radians = ClampPitch(m_pitch_radians);

    constexpr Vector3f k_up = {0, 1, 0};
    constexpr Vector3f k_local_forward = {0, 0, -1};
    const Quaternionf yaw_quat = Quaternionf::FromAxisAngleRadians(k_up, m_yaw_radians);
    const Vector3f world_forward = Opal::Normalize(yaw_quat * k_local_forward);
    const Vector3f world_right = Opal::Normalize(Opal::Cross(world_forward, k_up));
    const Quaternionf pitch_quat = Quaternionf::FromAxisAngleRadians(world_right, m_pitch_radians);
    SetRotation(Normalize(pitch_quat * yaw_quat));
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

void Rndr::FlyCamera::Tick(f32 delta_seconds)
{
    m_yaw_radians += m_delta_yaw_radians;
    m_yaw_radians = ClampYaw(m_yaw_radians);
    m_pitch_radians += m_delta_pitch_radians;
    m_pitch_radians = ClampPitch(m_pitch_radians);

    // Up is the same for both local camera coordinate system and in the world coordinate system
    constexpr Vector3f k_up = {0, 1, 0};
    constexpr Vector3f k_local_forward = {0, 0, -1};
    const Quaternionf yaw_quat = Quaternionf::FromAxisAngleRadians(k_up, m_yaw_radians);
    const Vector3f world_forward = Opal::Normalize(yaw_quat * k_local_forward);
    const Vector3f world_right = Opal::Normalize(Opal::Cross(world_forward, k_up));
    const Quaternionf pitch_quat = Quaternionf::FromAxisAngleRadians(world_right, m_pitch_radians);
    SetRotation(Normalize(pitch_quat * yaw_quat));

    const Vector3f final_forward = GetRotation() * k_local_forward;
    const Vector3f final_right = Opal::Normalize(Opal::Cross(final_forward, k_up));
    const Vector3f total_move_vector =
        delta_seconds * m_delta_move_forward * final_forward + delta_seconds * m_delta_move_right * final_right;
    SetPosition(GetPosition() + total_move_vector);

    m_delta_yaw_radians = 0;
    m_delta_pitch_radians = 0;
    m_delta_move_forward = 0;
    m_delta_move_right = 0;
}

Rndr::f32 Rndr::FlyCamera::ClampYaw(f32 yaw)
{
    if (yaw > Opal::k_pi_float)
    {
        yaw -= 2 * Opal::k_pi_float;
    }
    if (yaw < -Opal::k_pi_float)
    {
        yaw += 2 * Opal::k_pi_float;
    }
    return yaw;
}

Rndr::f32 Rndr::FlyCamera::ClampPitch(f32 pitch)
{
    const f32 pitch_degrees = Opal::Degrees(pitch);
    const f32 pitch_clamped_degrees = Opal::Clamp(pitch_degrees, -89.0f, 89.0f);
    return Opal::Radians(pitch_clamped_degrees);
}
