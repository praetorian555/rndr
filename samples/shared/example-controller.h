#pragma once

#include "rndr/application.hpp"
#include "rndr/fly-camera.hpp"
#include "rndr/types.hpp"

class ExampleController
{
public:
    ExampleController(Rndr::Application& app, Rndr::i32 screen_width, Rndr::i32 screen_height, const Rndr::FlyCameraDesc& camera_desc,
                      Rndr::f32 move_speed, Rndr::f32 yaw_speed, Rndr::f32 pitch_speed);
    ~ExampleController() = default;

    void Enable(bool enable);
    [[nodiscard]] bool IsEnabled() const;

    Rndr::f32 GetMoveSpeed() const { return m_move_speed; }
    Rndr::f32 GetYawSpeed() const { return m_yaw_speed; }
    Rndr::f32 GetPitchSpeed() const { return m_pitch_speed; }

    Rndr::Matrix4x4f GetViewTransform() const { return m_fly_camera.FromWorldToCamera(); }
    Rndr::Matrix4x4f GetProjectionTransform() const { return m_fly_camera.FromCameraToNDC(); }

    void SetMoveSpeed(Rndr::f32 speed) { m_move_speed = speed; }
    void SetYawSpeed(Rndr::f32 speed) { m_yaw_speed = speed; }
    void SetPitchSpeed(Rndr::f32 speed) { m_pitch_speed = speed; }

    void SetScreenSize(Rndr::i32 width, Rndr::i32 height);

    void Tick(Rndr::f32 delta_seconds);

private:
    void HandleLookVertical(Rndr::InputPrimitive, Rndr::f32 axis_value);
    void HandleLookVerticalButton(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool);
    void HandleLookHorizontal(Rndr::InputPrimitive, Rndr::f32 axis_value);
    void HandleLookHorizontalButton(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool);
    void HandleMoveForward(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool);
    void HandleMoveRight(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool);

    Rndr::FlyCamera m_fly_camera;
    Rndr::InputContext m_input_context;
    Rndr::f32 m_move_speed = 10.0f;
    Rndr::f32 m_yaw_speed = 0.5f;
    Rndr::f32 m_pitch_speed = 0.2f;
    Rndr::f32 m_forward_value = 0.0f;
    Rndr::f32 m_vertical_value = 0.0f;
    Rndr::f32 m_horizontal_value = 0.0f;
    Rndr::f32 m_right_value = 0.0f;
};
