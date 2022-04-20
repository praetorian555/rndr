#pragma once

#include "rndr/core/base.h"
#include "rndr/core/transform.h"

namespace rndr
{

enum class ProjectionType
{
    Orthographic,
    Perspective
};

struct ProjectionCameraProperties
{
    ProjectionType Projection = ProjectionType::Perspective;

    int ScreenWidth = 0;
    int ScreenHeight = 0;

    // Position of the near plane along z axis. Always positive value. In case of a perspective projection it can't be 0.
    real Near = 0.01;

    // Position of the near plane along z axis. Always positive value.
    real Far = 100.0;

    // Vertical field of view angle. Larger the value more things you can see. Too large values will cause distortion. This value is used
    // only for perspective projection.
    real VerticalFOV = 45.0;
};

/**
 * Represents user's point of view in the 3D world. It provides transforms from world space to normalized device coordinate (NDC) space.
 */
class ProjectionCamera
{

public:
    ProjectionCamera(const Transform& WorldToCamera, const ProjectionCameraProperties& Props);

    const Transform& FromWorldToCamera() const { return m_WorldToCamera; }
    const Transform& FromCameraToWorld() const { return m_CameraToWorld; }

    const Transform& FromWorldToNDC() const { return m_WorldToNDC; }
    const Transform& FromNDCToWorld() const { return m_NDCToWorld; }

#if defined RNDR_DEBUG
    const Transform& FromCameraToScreen() const { return m_CameraToScreen; }
    const Transform& FromScreenToCamera() const { return m_ScreenToCamera; }

    const Transform& FromScreenToNDC() const { return m_ScreenToNDC; }
    const Transform& FromNDCToScreen() const { return m_NDCToScreen; }
#endif  // RNDR_DEBUG

    void SetScreenSize(int ScreenWidth, int ScreenHeight);
    void SetNearAndFar(real Near, real Far);
    void SetVerticalFOV(real FOV);
    void SetProperties(const ProjectionCameraProperties& Props);
    void SetWorldToCamera(const Transform& WorldToCameraTransform);

    const ProjectionCameraProperties& GetProperties() const { return m_Props; }

private:
    Transform GetProjectionTransform() const;

private:
    Transform m_WorldToCamera;
    Transform m_CameraToWorld;
    Transform m_WorldToNDC;
    Transform m_NDCToWorld;

#if defined RNDR_DEBUG
    Transform m_CameraToScreen;
    Transform m_ScreenToCamera;
    Transform m_ScreenToNDC;
    Transform m_NDCToScreen;
#endif  // RNDR_DEBUG

    ProjectionCameraProperties m_Props;
};

Transform Orthographic(real Near, real Far);
Transform Perspective(real FOV, real AspectRatio, real Near, real Far);

}  // namespace rndr