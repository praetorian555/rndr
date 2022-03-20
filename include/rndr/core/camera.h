#pragma once

#include "rndr/core/base.h"
#include "rndr/core/transform.h"

namespace rndr
{

/**
 * Base class used to represent user's point of view in the 3D world. It provides transforms from
 * world space to normalized device coordinate (NDC) space.
 */
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

    /**
     * Update the transforms when the film width and height change.
     *
     * @param Width Width of a film plane in the world space.
     * @param Height Height of a film plane in the world space.
     */
    virtual void UpdateTransforms(int Width, int Height) = 0;

    int GetFilmWidth() const { return m_FilmWidth; }
    int GetFilmHeight() const { return m_FilmHeight; }

protected:
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

/**
 * Uses orhographic projection to go from camera space to screen space. Uses right-handed coordinate
 * system.
 */
class OrthographicCamera : public Camera
{
public:
    OrthographicCamera(const Transform& WorldToCamera,
                       int FilmWidth,
                       int FilmHeight,
                       real Near,
                       real Far);

    virtual void UpdateTransforms(int Width, int Height) override;

private:
    real m_Near;
    real m_Far;
};

/**
 * Uses perspective projection to go from camera space to screen space. Uses right-handed coordinate
 * system. Things become smaller as they are farther from the near plane.
 */
class PerspectiveCamera : public Camera
{
public:
    /**
     * Constructor.
     *
     * @param WorldToCamera Transform that transforms points from world space to camera local space.
     * Expects that in the camera space the camera is looking down the negative z-axis.
     * @param FilmWidth Width of a film plane in the world space.
     * @param FilmHeight Height of a film plane in the world space.
     * @param FOV Field of view angle. This is a vertical angle.
     * @param Near Near plane z value. Always positive. Can't be zero.
     * @param Far Far plane z value. Always positive.
     */
    PerspectiveCamera(const Transform& WorldToCamera,
                      int FilmWidth,
                      int FilmHeight,
                      real FOV,
                      real Near,
                      real Far);

    virtual void UpdateTransforms(int Width, int Height) override;

private:
    real m_FOV;
    real m_AspectRatio;
    real m_Near;
    real m_Far;
};

Transform Orthographic(real Near, real Far);
Transform Perspective(real FOV, real AspectRatio, real Near, real Far);

}  // namespace rndr