#pragma once

#include "math/transform.h"

#include "rndr/core/base.h"

namespace Rndr
{

enum class ProjectionType
{
    Orthographic,
    Perspective
};

struct ProjectionCameraDesc
{
    static constexpr int k_default_orthographic_width = 10;
    static constexpr real k_default_near_plane = MATH_REALC(0.01);
    static constexpr real k_default_far_plane = MATH_REALC(100.0);
    static constexpr real k_default_vertical_fov = MATH_REALC(45.0);

    ProjectionType projection = ProjectionType::Perspective;

    int orthographic_width = k_default_orthographic_width;

    /**
     * Position of the near plane along z axis. Always positive value. In case of a perspective
     * projection it can't be 0.
     */
    real near = k_default_near_plane;

    /** Position of the near plane along z axis. Always positive value. */
    real far = k_default_far_plane;

    /**
     * Vertical field of view angle. Larger the value more things you can see. Too large values will
     * cause distortion. This value is used only for perspective projection.
     */
    real vertical_fov = k_default_vertical_fov;
};

/**
 * Represents user's point of view in the 3D world. It provides transforms from world space to
 * normalized device coordinate (NDC) space.
 */
class ProjectionCamera
{

public:
    ProjectionCamera(const math::Transform& world_to_camera,
                     int screen_width,
                     int screen_height,
                     const ProjectionCameraDesc& desc);

    ProjectionCamera(const math::Point3& position,
                     const math::Rotator& rotation,
                     int screen_width,
                     int screen_height,
                     const ProjectionCameraDesc& desc);

    void SetPosition(const math::Point3& position);
    void SetRotation(const math::Rotator& rotation);
    void SetPositionAndRotation(const math::Point3& position, const math::Rotator& rotation);

    [[nodiscard]] math::Point3 GetPosition() const { return m_position; }
    [[nodiscard]] math::Rotator GetRotation() const { return m_rotation; }

    [[nodiscard]] const math::Transform& FromCameraToNDC() const { return m_camera_to_ndc; }
    [[nodiscard]] const math::Transform& FromNDCToCamera() const { return m_ndc_to_camera; }

    [[nodiscard]] const math::Transform& FromWorldToCamera() const { return m_world_to_camera; }
    [[nodiscard]] const math::Transform& FromCameraToWorld() const { return m_camera_to_world; }

    [[nodiscard]] const math::Transform& FromWorldToNDC() const { return m_world_to_ndc; }
    [[nodiscard]] const math::Transform& FromNDCToWorld() const { return m_ndc_to_world; }

    void SetScreenSize(int screen_width, int screen_height);
    void SetNearAndFar(real near, real far);
    void SetVerticalFOV(real fov);
    void UpdateTransforms();
    void SetWorldToCamera(const math::Transform& world_to_camera);

    [[nodiscard]] const ProjectionCameraDesc& GetDesc() const { return m_desc; }

private:
    // Private methods

    [[nodiscard]] math::Transform GetProjectionTransform() const;
    [[nodiscard]] real GetAspectRatio() const;

    // Private fields

    math::Transform m_world_to_camera;
    math::Transform m_camera_to_world;
    math::Transform m_camera_to_ndc;
    math::Transform m_ndc_to_camera;
    math::Transform m_world_to_ndc;
    math::Transform m_ndc_to_world;

    math::Point3 m_position;
    math::Rotator m_rotation;

    int m_screen_width;
    int m_screen_height;
    ProjectionCameraDesc m_desc;
};

}  // namespace Rndr
