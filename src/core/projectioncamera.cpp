#include "rndr/core/projectioncamera.h"

#include "math/projections.h"

rndr::ProjectionCamera::ProjectionCamera(const math::Transform& WorldToCamera,
                                         int ScreenWidth,
                                         int ScreenHeight,
                                         const ProjectionCameraProperties& Props)
    : m_WorldToCamera(WorldToCamera),
      m_CameraToWorld(m_WorldToCamera.GetInverse()),
      m_ScreenWidth(ScreenWidth),
      m_ScreenHeight(ScreenHeight),
      m_Props(Props)
{
    UpdateTransforms();
}

rndr::ProjectionCamera::ProjectionCamera(const math::Point3& Position,
                                         const math::Rotator& Rotation,
                                         int ScreenWidth,
                                         int ScreenHeight,
                                         const rndr::ProjectionCameraProperties& Props)
    : m_Position(Position),
      m_Rotation(Rotation),
      m_ScreenWidth(ScreenWidth),
      m_ScreenHeight(ScreenHeight),
      m_Props(Props)
{
    // TODO(Marko): Replace this with math::FromPositionAndRotation
    m_CameraToWorld = math::Translate(m_Position - math::Point3{}) * math::Rotate(m_Rotation);
    m_WorldToCamera = m_CameraToWorld.GetInverse();
    UpdateTransforms();
}

void rndr::ProjectionCamera::SetPosition(const math::Point3& Position)
{
    m_Position = Position;
    m_CameraToWorld = math::Translate(m_Position - math::Point3{}) * math::Rotate(m_Rotation);
    m_WorldToCamera = m_CameraToWorld.GetInverse();
    UpdateTransforms();
}

void rndr::ProjectionCamera::SetRotation(const math::Rotator& Rotation)
{
    m_Rotation = Rotation;
    m_CameraToWorld = math::Translate(m_Position - math::Point3{}) * math::Rotate(m_Rotation);
    m_WorldToCamera = m_CameraToWorld.GetInverse();
    UpdateTransforms();
}

void rndr::ProjectionCamera::SetPositionAndRotation(const math::Point3& Position,
                                                    const math::Rotator& Rotation)
{
    m_Position = Position;
    m_Rotation = Rotation;
    m_CameraToWorld = math::Translate(m_Position - math::Point3{}) * math::Rotate(m_Rotation);
    m_WorldToCamera = m_CameraToWorld.GetInverse();
    UpdateTransforms();
}

void rndr::ProjectionCamera::SetScreenSize(int ScreenWidth, int ScreenHeight)
{
    m_ScreenWidth = ScreenWidth;
    m_ScreenHeight = ScreenHeight;
    UpdateTransforms();
}

void rndr::ProjectionCamera::SetNearAndFar(real Near, real Far)
{
    m_Props.Near = Near;
    m_Props.Far = Far;
    UpdateTransforms();
}

void rndr::ProjectionCamera::SetVerticalFOV(real FOV)
{
    assert(m_Props.Projection == ProjectionType::Perspective);
    m_Props.VerticalFOV = FOV;
    UpdateTransforms();
}

void rndr::ProjectionCamera::UpdateTransforms()
{
    m_CameraToNDC = GetProjectionTransform();
    m_NDCToCamera = m_CameraToNDC.GetInverse();

    m_WorldToNDC = m_CameraToNDC * m_WorldToCamera;
    m_NDCToWorld = m_WorldToNDC.GetInverse();
}

void rndr::ProjectionCamera::SetWorldToCamera(const math::Transform& WorldToCameraTransform)
{
    m_WorldToCamera = WorldToCameraTransform;
    m_CameraToWorld = m_WorldToCamera.GetInverse();
    UpdateTransforms();
}

math::Transform rndr::ProjectionCamera::GetProjectionTransform() const
{
    const real AspectRatio = GetAspectRatio();
    if (m_Props.Projection == ProjectionType::Orthographic)
    {
        const real Width = static_cast<real>(m_Props.OrthographicWidth);
        const real Height = Width / AspectRatio;
#if RNDR_LEFT_HANDED
        return math::Transform{math::Orthographic_LH_N0(-Width / 2, Width / 2, -Height / 2,
                                                       Height / 2, m_Props.Near, m_Props.Far)};
#else
        return math::Transform{math::Orthographic_RH_N0(-Width / 2, Width / 2, -Height / 2,
                                                       Height / 2, m_Props.Near, m_Props.Far)};
#endif
    }
    else
    {
#if RNDR_LEFT_HANDED
        return math::Transform{
            math::Perspective_LH_N0(m_Props.VerticalFOV, AspectRatio, m_Props.Near, m_Props.Far)};
#else
        return math::Transform{
            math::Perspective_RH_N0(m_Props.VerticalFOV, AspectRatio, m_Props.Near, m_Props.Far)};
#endif
    }
}

real rndr::ProjectionCamera::GetAspectRatio() const
{
    if (m_ScreenHeight == 0)
    {
        return 1;
    }
    return static_cast<real>(m_ScreenWidth) / static_cast<real>(m_ScreenHeight);
}
