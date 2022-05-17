#pragma once

#include "rndr/core/base.h"
#include "rndr/core/math.h"

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
    ProjectionCamera(const math::Transform& WorldToCamera, const ProjectionCameraProperties& Props);

    const math::Transform& FromWorldToCamera() const { return m_WorldToCamera; }
    const math::Transform& FromCameraToWorld() const { return m_CameraToWorld; }

    const math::Transform& FromWorldToNDC() const { return m_WorldToNDC; }
    const math::Transform& FromNDCToWorld() const { return m_NDCToWorld; }

#if defined RNDR_DEBUG
    const math::Transform& FromCameraToScreen() const { return m_CameraToScreen; }
    const math::Transform& FromScreenToCamera() const { return m_ScreenToCamera; }

    const math::Transform& FromScreenToNDC() const { return m_ScreenToNDC; }
    const math::Transform& FromNDCToScreen() const { return m_NDCToScreen; }
#endif  // RNDR_DEBUG

    void SetScreenSize(int ScreenWidth, int ScreenHeight);
    void SetNearAndFar(real Near, real Far);
    void SetVerticalFOV(real FOV);
    void SetProperties(const ProjectionCameraProperties& Props);
    void SetWorldToCamera(const math::Transform& WorldToCameraTransform);

    const ProjectionCameraProperties& GetProperties() const { return m_Props; }

private:
    math::Transform GetProjectionTransform() const;

private:
    math::Transform m_WorldToCamera;
    math::Transform m_CameraToWorld;
    math::Transform m_WorldToNDC;
    math::Transform m_NDCToWorld;

#if defined RNDR_DEBUG
    math::Transform m_CameraToScreen;
    math::Transform m_ScreenToCamera;
    math::Transform m_ScreenToNDC;
    math::Transform m_NDCToScreen;
#endif  // RNDR_DEBUG

    ProjectionCameraProperties m_Props;
};

}  // namespace rndr