#include <catch2/catch2.hpp>

#include "rndr/projection-camera.hpp"
#include "rndr/projections.hpp"

TEST_CASE("Perspective", "ProjectionCamera")
{
    constexpr float k_vertical_fov = 90.0f;
    constexpr float k_near_plane = 0.1f;
    constexpr float k_far_plane = 1000.0f;
    constexpr int k_screen_width = 1920;
    constexpr int k_screen_height = 1080;
    constexpr float k_aspect_ratio = static_cast<float>(k_screen_width) / static_cast<float>(k_screen_height);

    const Rndr::Quaternionf identity_rotation = Rndr::Quaternionf::Identity();

    Rndr::ProjectionCameraDesc desc;
    desc.vertical_fov = k_vertical_fov;
    desc.near = k_near_plane;
    desc.far = k_far_plane;
    Rndr::ProjectionCamera camera(Rndr::Point3f{0.0f, 0.0f, 0.0f}, identity_rotation, k_screen_width, k_screen_height, desc);

    REQUIRE(camera.GetPosition() == Rndr::Point3f{0.0f, 0.0f, 0.0f});
    REQUIRE(camera.GetRotation() == identity_rotation);

    REQUIRE(camera.GetDesc().vertical_fov == k_vertical_fov);
    REQUIRE(camera.GetDesc().near == k_near_plane);
    REQUIRE(camera.GetDesc().far == k_far_plane);

    REQUIRE(camera.FromCameraToNDC() == Rndr::PerspectiveOpenGL(k_vertical_fov, k_aspect_ratio, k_near_plane, k_far_plane));

    const Rndr::Quaternionf test_rotation(Opal::Rotate(Rndr::Rotatorf{1.0f, 2.0f, 3.0f}));
    camera.SetPositionAndRotation(Rndr::Point3f{1.0f, 2.0f, 3.0f}, test_rotation);
    REQUIRE(camera.GetPosition() == Rndr::Point3f{1.0f, 2.0f, 3.0f});
    REQUIRE(camera.GetRotation() == test_rotation);
}

TEST_CASE("Orthographic", "ProjectionCamera")
{
    constexpr float k_near_plane = 0.0f;
    constexpr float k_far_plane = 1000.0f;
    constexpr int k_screen_width = 1920;
    constexpr int k_screen_height = 1080;
    constexpr int k_orthographic_width = 100;
    constexpr float k_aspect_ratio = static_cast<float>(k_screen_width) / static_cast<float>(k_screen_height);

    const Rndr::Quaternionf identity_rotation = Rndr::Quaternionf::Identity();

    Rndr::ProjectionCameraDesc desc;
    desc.near = k_near_plane;
    desc.far = k_far_plane;
    desc.projection = Rndr::ProjectionType::Orthographic;
    desc.orthographic_width = k_orthographic_width;
    Rndr::ProjectionCamera camera(Rndr::Point3f{0.0f, 0.0f, 0.0f}, identity_rotation, k_screen_width, k_screen_height, desc);

    REQUIRE(camera.GetPosition() == Rndr::Point3f{0.0f, 0.0f, 0.0f});
    REQUIRE(camera.GetRotation() == identity_rotation);

    // Add test checks for GetProperties.
    REQUIRE(camera.GetDesc().near == k_near_plane);
    REQUIRE(camera.GetDesc().far == k_far_plane);
    REQUIRE(camera.GetDesc().projection == Rndr::ProjectionType::Orthographic);
    REQUIRE(camera.GetDesc().orthographic_width == k_orthographic_width);

    const float half_width = k_orthographic_width / 2.0f;
    const float half_height = half_width / k_aspect_ratio;
    REQUIRE(camera.FromCameraToNDC() ==
            Rndr::OrthographicOpenGL(-half_width, half_width, -half_height, half_height, k_near_plane, k_far_plane));

    const Rndr::Rotatorf test_rotator{1.0f, 2.0f, 3.0f};
    const Rndr::Quaternionf test_rotation(Opal::Rotate(test_rotator));
    camera.SetPositionAndRotation(Rndr::Point3f{1.0f, 2.0f, 3.0f}, test_rotation);
    REQUIRE(camera.GetPosition() == Rndr::Point3f{1.0f, 2.0f, 3.0f});
    REQUIRE(camera.GetRotation() == test_rotation);

    const Rndr::Matrix4x4f ref = Opal::Translate(Rndr::Vector3f{1.0f, 2.0f, 3.0f}) * Opal::Rotate(test_rotation);
    REQUIRE(camera.FromCameraToWorld() == ref);
    REQUIRE(camera.FromWorldToCamera() == Opal::Inverse(ref));
}