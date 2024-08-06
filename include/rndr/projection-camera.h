#pragma once

#include "rndr/math.h"

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
    static constexpr float k_default_near_plane = 0.01f;
    static constexpr float k_default_far_plane = 100.0f;
    static constexpr float k_default_vertical_fov = 45.0f;

    ProjectionType projection = ProjectionType::Perspective;

    int orthographic_width = k_default_orthographic_width;

    /**
     * Position of the near plane along z axis. Always positive value. In case of a perspective
     * projection it can't be 0.
     */
    float near = k_default_near_plane;

    /** Position of the near plane along z axis. Always positive value. */
    float far = k_default_far_plane;

    /**
     * Vertical field of view angle. Larger the value more things you can see. Too large values will
     * cause distortion. This value is used only for perspective projection.
     */
    float vertical_fov = k_default_vertical_fov;
};

/**
 * Represents user's point of view in the 3D world. It provides transforms from world space to
 * normalized device coordinate (NDC) space.
 */
class ProjectionCamera
{

public:
    ProjectionCamera(const Matrix4x4f& world_to_camera, int screen_width, int screen_height, const ProjectionCameraDesc& desc);

    ProjectionCamera(const Point3f& position, const Rotatorf& rotation, int screen_width, int screen_height,
                     const ProjectionCameraDesc& desc);

    void SetPosition(const Point3f& position);
    void SetRotation(const Rotatorf& rotation);
    void SetPositionAndRotation(const Point3f& position, const Rotatorf& rotation);

    [[nodiscard]] Point3f GetPosition() const { return m_position; }
    [[nodiscard]] Rotatorf GetRotation() const { return m_rotation; }

    [[nodiscard]] const Matrix4x4f& FromCameraToNDC() const { return m_camera_to_ndc; }
    [[nodiscard]] const Matrix4x4f& FromNDCToCamera() const { return m_ndc_to_camera; }

    [[nodiscard]] const Matrix4x4f& FromWorldToCamera() const { return m_world_to_camera; }
    [[nodiscard]] const Matrix4x4f& FromCameraToWorld() const { return m_camera_to_world; }

    [[nodiscard]] const Matrix4x4f& FromWorldToNDC() const { return m_world_to_ndc; }
    [[nodiscard]] const Matrix4x4f& FromNDCToWorld() const { return m_ndc_to_world; }

    void SetScreenSize(int screen_width, int screen_height);
    void SetNearAndFar(float near, float far);
    void SetVerticalFOV(float fov);
    void UpdateTransforms();
    void SetWorldToCamera(const Matrix4x4f& world_to_camera);

    [[nodiscard]] const ProjectionCameraDesc& GetDesc() const { return m_desc; }

private:
    // Private methods

    [[nodiscard]] Matrix4x4f GetProjectionTransform() const;
    [[nodiscard]] float GetAspectRatio() const;

    // Private fields

    Matrix4x4f m_world_to_camera = Math::Identity<float>();
    Matrix4x4f m_camera_to_world = Math::Identity<float>();
    Matrix4x4f m_camera_to_ndc = Math::Identity<float>();
    Matrix4x4f m_ndc_to_camera = Math::Identity<float>();
    Matrix4x4f m_world_to_ndc = Math::Identity<float>();
    Matrix4x4f m_ndc_to_world = Math::Identity<float>();

    Point3f m_position = Point3f::Zero();
    Rotatorf m_rotation = Rotatorf::Zero();

    int m_screen_width;
    int m_screen_height;
    ProjectionCameraDesc m_desc;
};

}  // namespace Rndr