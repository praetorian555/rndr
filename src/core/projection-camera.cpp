#include "rndr/core/projection-camera.h"

#include "math/projections.h"

Rndr::ProjectionCamera::ProjectionCamera(const math::Transform& world_to_camera,
                                         int screen_width,
                                         int screen_height,
                                         const ProjectionCameraDesc& desc)
    : m_world_to_camera(world_to_camera),
      m_camera_to_world(world_to_camera.GetInverse()),
      m_screen_width(screen_width),
      m_screen_height(screen_height),
      m_desc(desc)
{
    UpdateTransforms();
}

Rndr::ProjectionCamera::ProjectionCamera(const math::Point3& position,
                                         const math::Rotator& rotation,
                                         int screen_width,
                                         int screen_height,
                                         const ProjectionCameraDesc& desc)
    : m_position(position),
      m_rotation(rotation),
      m_screen_width(screen_width),
      m_screen_height(screen_height),
      m_desc(desc)
{
    m_camera_to_world = math::RotateAndTranslate(m_rotation, m_position);
    m_world_to_camera = m_camera_to_world.GetInverse();
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetPosition(const math::Point3& position)
{
    m_position = position;
    m_camera_to_world = math::RotateAndTranslate(m_rotation, m_position);
    m_world_to_camera = m_camera_to_world.GetInverse();
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetRotation(const math::Rotator& rotation)
{
    m_rotation = rotation;
    m_camera_to_world = math::RotateAndTranslate(m_rotation, m_position);
    m_world_to_camera = m_camera_to_world.GetInverse();
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetPositionAndRotation(const math::Point3& position,
                                                    const math::Rotator& rotation)
{
    m_position = position;
    m_rotation = rotation;
    m_camera_to_world = math::RotateAndTranslate(m_rotation, m_position);
    m_world_to_camera = m_camera_to_world.GetInverse();
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetScreenSize(int screen_width, int screen_height)
{
    m_screen_width = screen_width;
    m_screen_height = screen_height;
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetNearAndFar(real near, real far)
{
    m_desc.near = near;
    m_desc.far = far;
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetVerticalFOV(real fov)
{
    assert(m_desc.projection == ProjectionType::Perspective);
    m_desc.vertical_fov = fov;
    UpdateTransforms();
}

void Rndr::ProjectionCamera::UpdateTransforms()
{
    m_camera_to_ndc = GetProjectionTransform();
    m_ndc_to_camera = m_camera_to_ndc.GetInverse();

    m_world_to_ndc = m_camera_to_ndc * m_world_to_camera;
    m_ndc_to_world = m_world_to_ndc.GetInverse();
}

void Rndr::ProjectionCamera::SetWorldToCamera(const math::Transform& world_to_camera)
{
    m_world_to_camera = world_to_camera;
    m_camera_to_world = m_world_to_camera.GetInverse();
    UpdateTransforms();
}

math::Transform Rndr::ProjectionCamera::GetProjectionTransform() const
{
    const real aspect_ratio = GetAspectRatio();
    if (m_desc.projection == ProjectionType::Orthographic)
    {
        const real width = static_cast<real>(m_desc.orthographic_width);
        const real height = width / aspect_ratio;
#if RNDR_DX11
        return math::Transform{math::Orthographic_LH_N0(-width / 2,
                                                        width / 2,
                                                        -height / 2,
                                                        height / 2,
                                                        m_desc.near,
                                                        m_desc.far)};
#elif RNDR_OPENGL
        return math::Transform{math::Orthographic_RH_N1(-width / 2,
                                                        width / 2,
                                                        -height / 2,
                                                        height / 2,
                                                        m_desc.near,
                                                        m_desc.far)};
#else
#error "Unknown render API"
#endif
    }
    else
    {
#if RNDR_DX11
        return math::Transform{
            math::Perspective_LH_N0(m_desc.vertical_fov, aspect_ratio, m_desc.near, m_desc.far)};
#elif RNDR_OPENGL
        return math::Transform{
            math::Perspective_RH_N1(m_desc.vertical_fov, aspect_ratio, m_desc.near, m_desc.far)};
#else
#error "Unknown render API"
#endif
    }
}

Rndr::real Rndr::ProjectionCamera::GetAspectRatio() const
{
    [[unlikely]] if (m_screen_height == 0)
    {
        return 1;
    }
    return static_cast<real>(m_screen_width) / static_cast<real>(m_screen_height);
}
