#pragma once

#include "math/transform.h"

#include "rndr/core/base.h"

namespace rndr
{

enum class ProjectionType
{
    Orthographic,
    Perspective
};

struct ProjectionCameraProperties
{
    static constexpr int kDefaultOrtographicWidth = 10;
    static constexpr real kDefaultNearPlane = 0.01f;
    static constexpr real kDefaultFarPlane = 100.0f;
    static constexpr real kDefaultVerticalFOV = 45.0f;

    ProjectionType Projection = ProjectionType::Perspective;

    int ScreenWidth = 0;
    int ScreenHeight = 0;

    int OrtographicWidth = kDefaultOrtographicWidth;

    // Position of the near plane along z axis. Always positive value. In case of a perspective
    // projection it can't be 0.
    real Near = kDefaultNearPlane;

    // Position of the near plane along z axis. Always positive value.
    real Far = kDefaultFarPlane;

    // Vertical field of view angle. Larger the value more things you can see. Too large values will
    // cause distortion. This value is used only for perspective projection.
    real VerticalFOV = kDefaultVerticalFOV;
};

/**
 * Represents user's point of view in the 3D world. It provides transforms from world space to
 * normalized device coordinate (NDC) space.
 */
class ProjectionCamera
{

public:
    ProjectionCamera(const math::Transform& WorldToCamera, const ProjectionCameraProperties& Props);

    [[nodiscard]] const math::Transform& FromWorldToCamera() const { return m_WorldToCamera; }
    [[nodiscard]] const math::Transform& FromCameraToWorld() const { return m_CameraToWorld; }

    [[nodiscard]] const math::Transform& FromWorldToNDC() const { return m_WorldToNDC; }
    [[nodiscard]] const math::Transform& FromNDCToWorld() const { return m_NDCToWorld; }

    void SetScreenSize(int ScreenWidth, int ScreenHeight);
    void SetNearAndFar(real Near, real Far);
    void SetVerticalFOV(real FOV);
    void SetProperties(const ProjectionCameraProperties& Props);
    void SetWorldToCamera(const math::Transform& WorldToCameraTransform);

    [[nodiscard]] const ProjectionCameraProperties& GetProperties() const { return m_Props; }

private:
    // Private methods

    [[nodiscard]] math::Transform GetProjectionTransform() const;
    [[nodiscard]] real GetAspectRatio() const;

    // Private fields

    math::Transform m_WorldToCamera;
    math::Transform m_CameraToWorld;
    math::Transform m_CameraToNDC;
    math::Transform m_NDCToCamera;
    math::Transform m_WorldToNDC;
    math::Transform m_NDCToWorld;

    ProjectionCameraProperties m_Props;
};

}  // namespace rndr
