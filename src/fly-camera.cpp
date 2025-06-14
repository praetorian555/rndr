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
    const Quaternionf rotation = GetRotation();
    const Vector3f forward = Opal::Normalize(rotation * Vector3f(0, 0, -1));
    const Point3f position = GetPosition();
    SetPosition(position + forward * amount_world_units);
}

void Rndr::FlyCamera::MoveRight(f32 amount_world_units)
{
    const Quaternionf rotation = GetRotation();
    const Vector3f forward = Opal::Normalize(rotation * Vector3f(0, 0, -1));
    const Vector3f up = Vector3f(0, 1, 0);
    const Vector3f right = Opal::Normalize(Opal::Cross(forward, up));
    const Point3f position = GetPosition();
    SetPosition(position + right * amount_world_units);
}

void Rndr::FlyCamera::AddYaw(f32 yaw_radians)
{
    m_yaw_radians += yaw_radians;
    if (m_yaw_radians > Opal::k_pi_float)
    {
        m_yaw_radians -= 2 * Opal::k_pi_float;
    }
    if (m_yaw_radians < -Opal::k_pi_float)
    {
        m_yaw_radians += 2 * Opal::k_pi_float;
    }
    const Quaternionf yaw_quat = Quaternionf::FromAxisAngleRadians(Rndr::Vector3f(0, 1, 0), m_yaw_radians);
    const Quaternionf roll_quat = Quaternionf::FromAxisAngleRadians(Rndr::Vector3f(1, 0, 0), m_roll_radians);
    SetRotation(yaw_quat * roll_quat);
}

void Rndr::FlyCamera::AddRoll(f32 roll_radians)
{
    m_roll_radians += roll_radians;
    f32 roll_degrees = Opal::Degrees(m_roll_radians);
    roll_degrees = Opal::Clamp(roll_degrees, -89.0f, 89.0f);
    m_roll_radians = Opal::Radians(roll_degrees);
    const Quaternionf yaw_quat = Quaternionf::FromAxisAngleRadians(Rndr::Vector3f(0, 1, 0), m_yaw_radians);
    const Quaternionf roll_quat = Quaternionf::FromAxisAngleRadians(Rndr::Vector3f(1, 0, 0), m_roll_radians);
    SetRotation(yaw_quat * roll_quat);
}
