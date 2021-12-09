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
    m_ScreenToNDC = Scale(2 / (real)FilmWidth, 2 / (real)FilmHeight, 1);
    m_NDCToScreen = Scale((real)FilmWidth / 2, (real)FilmHeight / 2, 1);
    m_WorldToNDC = m_ScreenToNDC * m_CameraToScreen * m_WorldToCamera;
    m_NDCToWorld = m_WorldToNDC.GetInverse();
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
}

rndr::PerspectiveCamera::PerspectiveCamera(const Transform& WorldToCamera,
                                           int FilmWidth,
                                           int FilmHeight,
                                           real FOV,
                                           real Near,
                                           real Far)
    : Camera(WorldToCamera, Perspective(FOV, Near, Far), FilmWidth, FilmHeight),
      m_FOV(FOV),
      m_Near(Near),
      m_Far(Far)
{
}

rndr::Transform rndr::Orthographic(real Near, real Far)
{
    return Scale(1, 1, 1 / (Far - Near)) * Translate(Vector3r{0, 0, -Near});
}

rndr::Transform rndr::Perspective(real FOV, real Near, real Far)
{
    // clang-format off
    Matrix4x4 Persp(
        1.0f, 0.0f,           0.0f,                 0.0f,
        0.0f, 1.0f,           0.0f,                 0.0f,
        0.0f, 0.0f,    Far / (Far - Near),    -Far * Near / (Far - Near),
        0.0f, 0.0f,           0.0f,                 1.0f
    );
    // clang-format on

    real InvFOV = 1 / std::tan(Radians(FOV) / 2);

    return Scale(InvFOV, InvFOV, 1) * Transform(Persp);
}
