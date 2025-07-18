#include "rndr/projection-camera.hpp"

#include "opal/math/transform.h"

#include "rndr/definitions.hpp"
#include "rndr/projections.hpp"

Rndr::ProjectionCamera::ProjectionCamera(const Matrix4x4f& world_to_camera, int screen_width, int screen_height,
                                         const ProjectionCameraDesc& desc)
    : m_world_to_camera(world_to_camera),
      m_camera_to_world(Opal::Inverse(world_to_camera)),
      m_screen_width(screen_width),
      m_screen_height(screen_height),
      m_desc(desc)
{
    UpdateTransforms();
}

Rndr::ProjectionCamera::ProjectionCamera(const Point3f& position, const Quaternionf& rotation, int screen_width, int screen_height,
                                         const ProjectionCameraDesc& desc)
    : m_position(position), m_rotation(rotation), m_screen_width(screen_width), m_screen_height(screen_height), m_desc(desc)
{
    m_camera_to_world = Opal::Translate(m_position) * Opal::Rotate(m_rotation);
    m_world_to_camera = Opal::Inverse(m_camera_to_world);
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetPosition(const Point3f& position)
{
    m_position = position;
    m_camera_to_world = Opal::Translate(m_position) * Opal::Rotate(m_rotation);
    m_world_to_camera = Opal::Inverse(m_camera_to_world);
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetRotation(const Quaternionf& rotation)
{
    m_rotation = rotation;
    m_camera_to_world = Opal::Translate(m_position) * Opal::Rotate(m_rotation);
    m_world_to_camera = Opal::Inverse(m_camera_to_world);
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetPositionAndRotation(const Point3f& position, const Quaternionf& rotation)
{
    m_position = position;
    m_rotation = rotation;
    m_camera_to_world = Opal::Translate(m_position) * Opal::Rotate(m_rotation);
    m_world_to_camera = Opal::Inverse(m_camera_to_world);
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetScreenSize(int screen_width, int screen_height)
{
    m_screen_width = screen_width;
    m_screen_height = screen_height;
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetNearAndFar(float near, float far)
{
    m_desc.near = near;
    m_desc.far = far;
    UpdateTransforms();
}

void Rndr::ProjectionCamera::SetVerticalFOV(float fov)
{
    RNDR_ASSERT(m_desc.projection == ProjectionType::Perspective, "Vertical FOV can only be set for perspective projections!");
    m_desc.vertical_fov = fov;
    UpdateTransforms();
}

void Rndr::ProjectionCamera::UpdateTransforms()
{
    m_camera_to_ndc = GetProjectionTransform();
    m_ndc_to_camera = Opal::Inverse(m_camera_to_ndc);

    m_world_to_ndc = m_camera_to_ndc * m_world_to_camera;
    m_ndc_to_world = Opal::Inverse(m_world_to_ndc);
}

void Rndr::ProjectionCamera::SetWorldToCamera(const Matrix4x4f& world_to_camera)
{
    m_world_to_camera = world_to_camera;
    m_camera_to_world = Opal::Inverse(m_world_to_camera);
    UpdateTransforms();
}

Rndr::Matrix4x4f Rndr::ProjectionCamera::GetProjectionTransform() const
{
    const float aspect_ratio = GetAspectRatio();
    if (m_desc.projection == ProjectionType::Orthographic)
    {
        const float width = static_cast<float>(m_desc.orthographic_width);
        const float height = width / aspect_ratio;
#if RNDR_OPENGL
        return OrthographicOpenGL(-width / 2, width / 2, -height / 2, height / 2, m_desc.near, m_desc.far);
#else
#error "Unknown render API"
#endif
    }
    else
    {
#if RNDR_OPENGL
        return PerspectiveOpenGL(m_desc.vertical_fov, aspect_ratio, m_desc.near, m_desc.far);
#else
#error "Unknown render API"
#endif
    }
}

float Rndr::ProjectionCamera::GetAspectRatio() const
{
    [[unlikely]] if (m_screen_height == 0)
    {
        return 1;
    }
    return static_cast<float>(m_screen_width) / static_cast<float>(m_screen_height);
}
