#include "rndr/core/projectioncamera.h"

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

    math::Transform CameraToScreen = GetProjectionTransform();

    math::Transform ScreenToNDC;
    if (Props.Projection == ProjectionType::Orthographic)
    {
        ScreenToNDC = math::Scale(2 / (real)m_Props.ScreenWidth, 2 / (real)m_Props.ScreenHeight, 1);
    }

#if defined RNDR_DEBUG
    m_CameraToScreen = CameraToScreen;
    m_ScreenToCamera = m_CameraToScreen.GetInverse();
    m_ScreenToNDC = ScreenToNDC;
    m_NDCToScreen = m_ScreenToNDC.GetInverse();
#endif

    m_WorldToNDC = ScreenToNDC * CameraToScreen * m_WorldToCamera;
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
    if (m_Props.Projection == ProjectionType::Orthographic)
    {
        return (math::Transform)math::Orhographic_LH_N0(-1, 1, -1, 1, m_Props.Near, m_Props.Far);
    }
    else
    {
        real AspectRatio = (real)m_Props.ScreenWidth / m_Props.ScreenHeight;
        return (math::Transform)math::Perspective_LH_N0(m_Props.VerticalFOV, AspectRatio, m_Props.Near, m_Props.Far);
    }
}
