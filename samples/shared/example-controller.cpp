#include "example-controller.h"

ExampleController::ExampleController(Rndr::Application& app, Rndr::i32 screen_width, Rndr::i32 screen_height,
                                     const Rndr::FlyCameraDesc& camera_desc, Rndr::f32 move_speed, Rndr::f32 yaw_speed,
                                     Rndr::f32 pitch_speed)
    : m_fly_camera(screen_width, screen_height, camera_desc),
      m_input_context("Example Controller Input Context"),
      m_move_speed(move_speed),
      m_yaw_speed(yaw_speed),
      m_pitch_speed(pitch_speed)
{
    m_input_context.AddAction("MoveForward")
        .OnButton([this](Rndr::Trigger trigger, bool)
        {
            if (trigger == Rndr::Trigger::Pressed)
            {
                m_forward_value = m_move_speed;
            }
            else if (trigger == Rndr::Trigger::Released)
            {
                m_forward_value = 0;
            }
        })
        .Bind(Rndr::Key::W, Rndr::Trigger::Pressed)
        .Bind(Rndr::Key::W, Rndr::Trigger::Released);

    m_input_context.AddAction("MoveBackward")
        .OnButton([this](Rndr::Trigger trigger, bool)
        {
            if (trigger == Rndr::Trigger::Pressed)
            {
                m_forward_value = -m_move_speed;
            }
            else if (trigger == Rndr::Trigger::Released)
            {
                m_forward_value = 0;
            }
        })
        .Bind(Rndr::Key::S, Rndr::Trigger::Pressed)
        .Bind(Rndr::Key::S, Rndr::Trigger::Released);

    m_input_context.AddAction("MoveLeft")
        .OnButton([this](Rndr::Trigger trigger, bool)
        {
            if (trigger == Rndr::Trigger::Pressed)
            {
                m_right_value = -m_move_speed;
            }
            else if (trigger == Rndr::Trigger::Released)
            {
                m_right_value = 0;
            }
        })
        .Bind(Rndr::Key::A, Rndr::Trigger::Pressed)
        .Bind(Rndr::Key::A, Rndr::Trigger::Released);

    m_input_context.AddAction("MoveRight")
        .OnButton([this](Rndr::Trigger trigger, bool)
        {
            if (trigger == Rndr::Trigger::Pressed)
            {
                m_right_value = m_move_speed;
            }
            else if (trigger == Rndr::Trigger::Released)
            {
                m_right_value = 0;
            }
        })
        .Bind(Rndr::Key::D, Rndr::Trigger::Pressed)
        .Bind(Rndr::Key::D, Rndr::Trigger::Released);

    m_input_context.AddAction("LookAroundVert")
        .OnButton([this](Rndr::Trigger trigger, bool)
        {
            if (trigger == Rndr::Trigger::Pressed)
            {
                m_vertical_value = m_pitch_speed;
            }
            else if (trigger == Rndr::Trigger::Released)
            {
                m_vertical_value = 0;
            }
        })
        .Bind(Rndr::Key::UpArrow, Rndr::Trigger::Pressed)
        .Bind(Rndr::Key::UpArrow, Rndr::Trigger::Released);

    m_input_context.AddAction("LookAroundVertDown")
        .OnButton([this](Rndr::Trigger trigger, bool)
        {
            if (trigger == Rndr::Trigger::Pressed)
            {
                m_vertical_value = -m_pitch_speed;
            }
            else if (trigger == Rndr::Trigger::Released)
            {
                m_vertical_value = 0;
            }
        })
        .Bind(Rndr::Key::DownArrow, Rndr::Trigger::Pressed)
        .Bind(Rndr::Key::DownArrow, Rndr::Trigger::Released);

    m_input_context.AddAction("LookAroundVertMouse")
        .OnMousePosition([this](Rndr::MouseAxis axis, Rndr::f32 delta)
        {
            if (axis == Rndr::MouseAxis::Y)
            {
                m_fly_camera.AddPitch(-m_pitch_speed * delta);
            }
        })
        .Bind(Rndr::MouseAxis::Y);

    m_input_context.AddAction("LookAroundHorzRight")
        .OnButton([this](Rndr::Trigger trigger, bool)
        {
            if (trigger == Rndr::Trigger::Pressed)
            {
                m_horizontal_value = -m_yaw_speed;
            }
            else if (trigger == Rndr::Trigger::Released)
            {
                m_horizontal_value = 0;
            }
        })
        .Bind(Rndr::Key::RightArrow, Rndr::Trigger::Pressed)
        .Bind(Rndr::Key::RightArrow, Rndr::Trigger::Released);

    m_input_context.AddAction("LookAroundHorzLeft")
        .OnButton([this](Rndr::Trigger trigger, bool)
        {
            if (trigger == Rndr::Trigger::Pressed)
            {
                m_horizontal_value = m_yaw_speed;
            }
            else if (trigger == Rndr::Trigger::Released)
            {
                m_horizontal_value = 0;
            }
        })
        .Bind(Rndr::Key::LeftArrow, Rndr::Trigger::Pressed)
        .Bind(Rndr::Key::LeftArrow, Rndr::Trigger::Released);

    m_input_context.AddAction("LookAroundHorzMouse")
        .OnMousePosition([this](Rndr::MouseAxis axis, Rndr::f32 delta)
        {
            if (axis == Rndr::MouseAxis::X)
            {
                m_fly_camera.AddYaw(-m_yaw_speed * delta);
            }
        })
        .Bind(Rndr::MouseAxis::X);

    app.GetInputSystemChecked().PushContext(m_input_context);
}

void ExampleController::Enable(bool enable)
{
    m_input_context.SetEnabled(enable);
}

bool ExampleController::IsEnabled() const
{
    return m_input_context.IsEnabled();
}

void ExampleController::SetScreenSize(Rndr::i32 width, Rndr::i32 height)
{
    m_fly_camera.SetScreenSize(width, height);
}

void ExampleController::Tick(Rndr::f32 delta_seconds)
{
    if (m_vertical_value != 0)
    {
        m_fly_camera.AddPitch(m_vertical_value);
    }
    if (m_horizontal_value != 0)
    {
        m_fly_camera.AddYaw(m_horizontal_value);
    }
    if (m_forward_value != 0)
    {
        m_fly_camera.MoveForward(m_forward_value);
    }
    if (m_right_value != 0)
    {
        m_fly_camera.MoveRight(m_right_value);
    }

    m_fly_camera.Tick(delta_seconds);
}
