#include "rndr/core/projectioncamera.h"

#include "math/projections.h"

rndr::ProjectionCamera::ProjectionCamera(const math::Transform& WorldToCamera, const ProjectionCameraProperties& Props)
    : m_WorldToCamera(WorldToCamera), m_Props(Props)
{
    m_CameraToWorld = m_WorldToCamera.GetInverse();
    SetProperties(m_Props);
}

void rndr::ProjectionCamera::SetScreenSize(int ScreenWidth, int ScreenHeight)
{
    ProjectionCameraProperties Props = m_Props;
    Props.ScreenWidth = ScreenWidth;
    Props.ScreenHeight = ScreenHeight;
    SetProperties(Props);
}

void rndr::ProjectionCamera::SetNearAndFar(real Near, real Far)
{
    ProjectionCameraProperties Props = m_Props;
    Props.Near = Near;
    Props.Far = Far;
    SetProperties(Props);
}

void rndr::ProjectionCamera::SetVerticalFOV(real FOV)
{
    assert(m_Props.Projection == ProjectionType::Perspective);
    ProjectionCameraProperties Props = m_Props;
    Props.VerticalFOV = FOV;
    SetProperties(Props);
}

void rndr::ProjectionCamera::SetProperties(const ProjectionCameraProperties& Props)
{
    m_Props = Props;

    m_CameraToNDC = GetProjectionTransform();
    m_NDCToCamera = m_CameraToNDC.GetInverse();

    m_WorldToNDC = m_CameraToNDC * m_WorldToCamera;
    m_NDCToWorld = m_WorldToNDC.GetInverse();
}

void rndr::ProjectionCamera::SetWorldToCamera(const math::Transform& WorldToCameraTransform)
{
    m_WorldToCamera = WorldToCameraTransform;
    m_CameraToWorld = m_WorldToCamera.GetInverse();
    SetProperties(m_Props);
}

math::Transform rndr::ProjectionCamera::GetProjectionTransform() const
{
    const real AspectRatio = GetAspectRatio();
    if (m_Props.Projection == ProjectionType::Orthographic)
    {
        const real Width = m_Props.OrtographicWidth;
        const real Height = Width / AspectRatio;
#if RNDR_LEFT_HANDED
        return (math::Transform)math::Orhographic_LH_N0(-Width / 2, Width / 2, -Height / 2, Height / 2, m_Props.Near, m_Props.Far);
#else
        return (math::Transform)math::Orhographic_RH_N0(-Width / 2, Width / 2, -Height / 2, Height / 2, m_Props.Near, m_Props.Far);
#endif
    }
    else
    {
#if RNDR_LEFT_HANDED
        return (math::Transform)math::Perspective_LH_N0(m_Props.VerticalFOV, AspectRatio, m_Props.Near, m_Props.Far);
#else
        return (math::Transform)math::Perspective_RH_N0(m_Props.VerticalFOV, AspectRatio, m_Props.Near, m_Props.Far);
#endif
    }
}

real rndr::ProjectionCamera::GetAspectRatio() const
{
    if (m_Props.ScreenHeight == 0)
    {
        return 1;
    }
    return (real)m_Props.ScreenWidth / m_Props.ScreenHeight;
}
