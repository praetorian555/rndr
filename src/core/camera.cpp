#include "rndr/core/camera.h"

rndr::Camera::Camera(const Transform& WorldToCamera,
                     const Transform& CameraToScreen,
                     int FilmWidth,
                     int FilmHeight)
    : m_WorldToCamera(WorldToCamera),
      m_CameraToScreen(CameraToScreen),
      m_FilmWidth(FilmWidth),
      m_FilmHeight(FilmHeight)
{
    m_CameraToWorld = m_WorldToCamera.GetInverse();
    m_ScreenToCamera = m_ScreenToCamera.GetInverse();
}

rndr::OrthographicCamera::OrthographicCamera(const Transform& WorldToCamera,
                                             int FilmWidth,
                                             int FilmHeight,
                                             real Near,
                                             real Far)
    : Camera(WorldToCamera, Orthographic(Near, Far), FilmWidth, FilmHeight),
      m_Near(Near),
      m_Far(Far)
{
    UpdateTransforms(FilmWidth, FilmHeight);
}

void rndr::OrthographicCamera::UpdateTransforms(int Width, int Height)
{
    m_FilmWidth = Width;
    m_FilmHeight = Height;
    m_ScreenToNDC = Scale(2 / (real)m_FilmWidth, 2 / (real)m_FilmHeight, 1);
    m_NDCToScreen = Scale((real)m_FilmWidth / 2, (real)m_FilmHeight / 2, 1);
    m_WorldToNDC = m_ScreenToNDC * m_CameraToScreen * m_WorldToCamera;
    m_NDCToWorld = m_WorldToNDC.GetInverse();
}

rndr::PerspectiveCamera::PerspectiveCamera(const Transform& WorldToCamera,
                                           int FilmWidth,
                                           int FilmHeight,
                                           real FOV,
                                           real Near,
                                           real Far)
    : Camera(WorldToCamera,
             Perspective(FOV, (real)FilmWidth / FilmHeight, Near, Far),
             FilmWidth,
             FilmHeight),
      m_FOV(FOV),
      m_AspectRatio((real)FilmWidth / FilmHeight),
      m_Near(Near),
      m_Far(Far)
{
    UpdateTransforms(FilmWidth, FilmHeight);
}

void rndr::PerspectiveCamera::UpdateTransforms(int Width, int Height)
{
    if (Width == 0 || Height == 0)
    {
        return;
    }

    m_AspectRatio = (real)Width / Height;

    m_FilmWidth = Width;
    m_FilmHeight = Height;
    m_CameraToScreen = Perspective(m_FOV, m_AspectRatio, m_Near, m_Far);
    m_ScreenToCamera = m_CameraToScreen.GetInverse();

    // Perspective projection doesn't need any scaling of x and y

    m_WorldToNDC = m_ScreenToNDC * m_CameraToScreen * m_WorldToCamera;
    m_NDCToWorld = m_WorldToNDC.GetInverse();
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
    Matrix4x4 Persp(
        1.0, 0.0,           0.0,                 0.0,
        0.0, 1.0,           0.0,                 0.0,
        0.0, 0.0,    -Far / (Far - Near),    -Far * Near / (Far - Near),
        0.0, 0.0,           -1.0,                 0.0
    );
    // clang-format on

    const real InvFOV = 1 / std::tan(Radians(FOV) / 2);
    const real OneOverAspectRatio = 1 / AspectRatio;

    return Scale(OneOverAspectRatio * InvFOV, InvFOV, 1) * Transform(Persp);
}
