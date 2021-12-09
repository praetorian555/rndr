#pragma once

#include "rndr/core/rndr.h"
#include "rndr/core/transform.h"

namespace rndr
{

class Camera
{

public:
    Camera(const Transform& WorldToCamera,
           const Transform& CameraToScreen,
           int FilmWidth,
           int FilmHeight);

    const Transform& FromWorldToCamera() const { return m_WorldToCamera; }
    const Transform& FromCameraToWorld() const { return m_CameraToWorld; }

    const Transform& FromCameraToScreen() const { return m_CameraToScreen; }
    const Transform& FromScreenToCamera() const { return m_ScreenToCamera; }

    const Transform& FromScreenToNDC() const { return m_ScreenToNDC; }
    const Transform& FromNDCToScreen() const { return m_NDCToScreen; }

    const Transform& FromWorldToNDC() const { return m_WorldToNDC; }
    const Transform& FromNDCToWorld() const { return m_NDCToWorld; }

private:
    Transform m_WorldToCamera;
    Transform m_CameraToWorld;
    Transform m_CameraToScreen;
    Transform m_ScreenToCamera;
    Transform m_ScreenToNDC;
    Transform m_NDCToScreen;
    Transform m_WorldToNDC;
    Transform m_NDCToWorld;
    int m_FilmWidth;   // Width of film in world space
    int m_FilmHeight;  // Height of film in world space
};

// Uses orhographic projection to go from camera space to screen space.
class OrthographicCamera : public Camera
{
public:
    OrthographicCamera(const Transform& WorldToCamera,
                       int FilmWidth,
                       int FilmHeight,
                       real Near,
                       real Far);

private:
    real m_Near;
    real m_Far;
};

// Uses perspective projection to go from camera space to screen space.
class PerspectiveCamera : public Camera
{
public:
    PerspectiveCamera(const Transform& WorldToCamera,
                      int FilmWidth,
                      int FilmHeight,
                      real FOV,
                      real Near,
                      real Far);

private:
    real m_FOV;
    real m_Near;
    real m_Far;
};

Transform Orthographic(real Near, real Far);
Transform Perspective(real FOV, real Near, real Far);

}  // namespace rndr