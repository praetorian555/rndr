#include <catch2/catch_test_macros.hpp>

// Generate unit tests for ProjectionCamera class using catch2.

#include "math/math.h"

#include "rndr/core/projection-camera.h"

TEST_CASE("Perspective", "ProjectionCamera")
{
    constexpr float kVerticalFOV = 90.0f;
    constexpr float kNearPlane = 0.1f;
    constexpr float kFarPlane = 1000.0f;
    constexpr int kScreenWidth = 1920;
    constexpr int kScreenHeight = 1080;
    constexpr float kAspectRatio =
        static_cast<float>(kScreenWidth) / static_cast<float>(kScreenHeight);

    // Replace the VerticalFOV literal with a constant.
    rndr::ProjectionCameraProperties Props;
    Props.VerticalFOV = kVerticalFOV;
    Props.Near = kNearPlane;
    Props.Far = kFarPlane;
    rndr::ProjectionCamera Camera(math::Point3{0.0f, 0.0f, 0.0f}, math::Rotator{0.0f, 0.0f, 0.0f},
                                  kScreenWidth, kScreenHeight, Props);

    REQUIRE(Camera.GetPosition() == math::Point3{0.0f, 0.0f, 0.0f});
    REQUIRE(Camera.GetRotation() == math::Rotator{0.0f, 0.0f, 0.0f});

    // Add test checks for GetProperties.
    REQUIRE(Camera.GetProperties().VerticalFOV == kVerticalFOV);
    REQUIRE(Camera.GetProperties().Near == kNearPlane);
    REQUIRE(Camera.GetProperties().Far == kFarPlane);

    REQUIRE(Camera.FromCameraToNDC().GetMatrix() ==
            math::Perspective_LH_N0(kVerticalFOV, kAspectRatio, kNearPlane, kFarPlane));

    Camera.SetPositionAndRotation(math::Point3{1.0f, 2.0f, 3.0f}, math::Rotator{1.0f, 2.0f, 3.0f});
    REQUIRE(Camera.GetPosition() == math::Point3{1.0f, 2.0f, 3.0f});
    REQUIRE(Camera.GetRotation() == math::Rotator{1.0f, 2.0f, 3.0f});
}

// Write a test same as the above one but this time make an orthographic camera.
TEST_CASE("Orthographic", "ProjectionCamera")
{
    constexpr float kNearPlane = 0.0f;
    constexpr float kFarPlane = 1000.0f;
    constexpr int kScreenWidth = 1920;
    constexpr int kScreenHeight = 1080;
    constexpr int kOrthographicWidth = 100;
    constexpr float kAspectRatio =
        static_cast<float>(kScreenWidth) / static_cast<float>(kScreenHeight);

    // Replace the VerticalFOV literal with a constant.
    rndr::ProjectionCameraProperties Props;
    Props.Near = kNearPlane;
    Props.Far = kFarPlane;
    Props.Projection = rndr::ProjectionType::Orthographic;
    Props.OrthographicWidth = kOrthographicWidth;
    rndr::ProjectionCamera Camera(math::Point3{0.0f, 0.0f, 0.0f}, math::Rotator{0.0f, 0.0f, 0.0f},
                                  kScreenWidth, kScreenHeight, Props);

    REQUIRE(Camera.GetPosition() == math::Point3{0.0f, 0.0f, 0.0f});
    REQUIRE(Camera.GetRotation() == math::Rotator{0.0f, 0.0f, 0.0f});

    // Add test checks for GetProperties.
    REQUIRE(Camera.GetProperties().Near == kNearPlane);
    REQUIRE(Camera.GetProperties().Far == kFarPlane);
    REQUIRE(Camera.GetProperties().Projection == rndr::ProjectionType::Orthographic);
    REQUIRE(Camera.GetProperties().OrthographicWidth == kOrthographicWidth);

    const float HalfWidth = kOrthographicWidth / 2.0f;
    const float HalfHeight = HalfWidth / kAspectRatio;
    REQUIRE(Camera.FromCameraToNDC().GetMatrix() == math::Orthographic_LH_N0(-HalfWidth, HalfWidth,
                                                                            -HalfHeight, HalfHeight,
                                                                            kNearPlane, kFarPlane));

    // Write test for SetPositionAndRotation but both position and rotation are not zero.
    Camera.SetPositionAndRotation(math::Point3{1.0f, 2.0f, 3.0f}, math::Rotator{1.0f, 2.0f, 3.0f});
    REQUIRE(Camera.GetPosition() == math::Point3{1.0f, 2.0f, 3.0f});
    REQUIRE(Camera.GetRotation() == math::Rotator{1.0f, 2.0f, 3.0f});

    math::Transform Ref = math::Translate(math::Vector3{1.0f, 2.0f, 3.0f}) *
                          math::Rotate(math::Rotator{1.0f, 2.0f, 3.0f});
    REQUIRE(Camera.FromCameraToWorld() == Ref);
    REQUIRE(Camera.FromWorldToCamera().GetMatrix() == Ref.GetInverse());
}
