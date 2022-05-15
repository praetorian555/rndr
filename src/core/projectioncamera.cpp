#include "rndr/core/projectioncamera.h"

rndr::ProjectionCamera::ProjectionCamera(const Transform& WorldToCamera, const ProjectionCameraProperties& Props)
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

    Transform CameraToScreen = GetProjectionTransform();

    Transform ScreenToNDC;
    if (Props.Projection == ProjectionType::Orthographic)
    {
       ScreenToNDC = Scale(2 / (real)m_Props.ScreenWidth, 2 / (real)m_Props.ScreenHeight, 1);
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

void rndr::ProjectionCamera::SetWorldToCamera(const Transform& WorldToCameraTransform)
{
    m_WorldToCamera = WorldToCameraTransform;
    m_CameraToWorld = m_WorldToCamera.GetInverse();
    SetProperties(m_Props);
}

rndr::Transform rndr::ProjectionCamera::GetProjectionTransform() const
{
    if (m_Props.Projection == ProjectionType::Orthographic)
    {
        return Orthographic(m_Props.Near, m_Props.Far);
    }
    else
    {
        real AspectRatio = (real)m_Props.ScreenWidth / m_Props.ScreenHeight;
        return Perspective(m_Props.VerticalFOV, AspectRatio, m_Props.Near, m_Props.Far);
    }
}

rndr::Transform rndr::Orthographic(real Near, real Far)
{
    return Scale(1, 1, 1 / (Far - Near)) * Translate(Vector3r{0, 0, -Near});
}

rndr::Transform rndr::Perspective(real FOV, real AspectRatio, real Near, real Far)
{
    assert(Near > 0);
    assert(Far > 0);
    assert(AspectRatio != 0);

    // Camera in camera space looks down the -z axis (that's the reason for minuses). We normalize z
    // to be between 0 and 1 (that is why we divide by f - n).
    // clang-format off
    math::Matrix4x4 Persp(
        1.0, 0.0,           0.0,                 0.0,
        0.0, 1.0,           0.0,                 0.0,
        0.0, 0.0,    -Far / (Far - Near),    -Far * Near / (Far - Near),
        0.0, 0.0,           -1.0,                 0.0
    );
    // clang-format on

    const real InvFOV = 1 / std::tan(math::Radians(FOV) / 2);
    const real OneOverAspectRatio = 1 / AspectRatio;

    return Scale(OneOverAspectRatio * InvFOV, InvFOV, 1) * Transform(Persp);
}
