#if RNDR_OLD_INPUT_SYSTEM

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
    m_input_context.SetEnabled(false);

    using IB = Rndr::InputBinding;
    using IT = Rndr::InputTrigger;
    using IP = Rndr::InputPrimitive;

    Opal::DynamicArray<IB> forward_bindings;
    forward_bindings.PushBack(
        IB::CreateKeyboardButtonBinding(IP::W, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleMoveForward)));
    forward_bindings.PushBack(
        IB::CreateKeyboardButtonBinding(IP::W, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleMoveForward)));
    forward_bindings.PushBack(
        IB::CreateKeyboardButtonBinding(IP::S, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleMoveForward), -1));
    forward_bindings.PushBack(
        IB::CreateKeyboardButtonBinding(IP::S, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleMoveForward)));
    m_input_context.AddAction("MoveForward", forward_bindings);

    Opal::DynamicArray<IB> right_bindings;
    right_bindings.PushBack(
        IB::CreateKeyboardButtonBinding(IP::A, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleMoveRight), -1));
    right_bindings.PushBack(
        IB::CreateKeyboardButtonBinding(IP::A, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleMoveRight)));
    right_bindings.PushBack(
        IB::CreateKeyboardButtonBinding(IP::D, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleMoveRight)));
    right_bindings.PushBack(
        IB::CreateKeyboardButtonBinding(IP::D, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleMoveRight)));
    m_input_context.AddAction("MoveRight", right_bindings);

    Opal::DynamicArray<IB> vert_bindings;
    vert_bindings.PushBack(
        IB::CreateKeyboardButtonBinding(IP::UpArrow, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleLookVerticalButton)));
    vert_bindings.PushBack(
        IB::CreateKeyboardButtonBinding(IP::UpArrow, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleLookVerticalButton)));
    vert_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::DownArrow, IT::ButtonPressed,
                                                           RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleLookVerticalButton), -1));
    vert_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::DownArrow, IT::ButtonReleased,
                                                           RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleLookVerticalButton)));
    vert_bindings.PushBack(
        IB::CreateMousePositionBinding(IP::Mouse_AxisY, RNDR_BIND_INPUT_MOUSE_POSITION_CALLBACK(this, HandleLookVertical)));
    m_input_context.AddAction("LookAroundVert", vert_bindings);

    Opal::DynamicArray<IB> horz_bindings;
    horz_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::RightArrow, IT::ButtonPressed,
                                                           RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleLookHorizontalButton), -1));
    horz_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::RightArrow, IT::ButtonReleased,
                                                           RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleLookHorizontalButton)));
    horz_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::LeftArrow, IT::ButtonPressed,
                                                           RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleLookHorizontalButton)));
    horz_bindings.PushBack(IB::CreateKeyboardButtonBinding(IP::LeftArrow, IT::ButtonReleased,
                                                           RNDR_BIND_INPUT_BUTTON_CALLBACK(this, HandleLookHorizontalButton)));
    horz_bindings.PushBack(
        IB::CreateMousePositionBinding(IP::Mouse_AxisX, RNDR_BIND_INPUT_MOUSE_POSITION_CALLBACK(this, HandleLookHorizontal)));
    m_input_context.AddAction("LookAroundHorz", horz_bindings);

    app.GetInputSystemChecked().PushContext(Opal::Ref(&m_input_context));
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

void ExampleController::HandleLookVertical(Rndr::InputPrimitive, Rndr::f32 axis_value)
{
    m_fly_camera.AddPitch(-m_pitch_speed * axis_value);
}

void ExampleController::HandleLookVerticalButton(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool)
{
    if (trigger == Rndr::InputTrigger::ButtonPressed)
    {
        m_vertical_value = m_pitch_speed * value;
    }
    else if (trigger == Rndr::InputTrigger::ButtonReleased)
    {
        m_vertical_value = 0;
    }
}

void ExampleController::HandleLookHorizontal(Rndr::InputPrimitive, Rndr::f32 axis_value)
{
    m_fly_camera.AddYaw(-m_yaw_speed * axis_value);
}

void ExampleController::HandleLookHorizontalButton(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool)
{
    if (trigger == Rndr::InputTrigger::ButtonPressed)
    {
        m_horizontal_value = m_yaw_speed * value;
    }
    else if (trigger == Rndr::InputTrigger::ButtonReleased)
    {
        m_horizontal_value = 0;
    }
}

void ExampleController::HandleMoveForward(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool)
{
    if (trigger == Rndr::InputTrigger::ButtonPressed)
    {
        m_forward_value = m_move_speed * value;
    }
    else if (trigger == Rndr::InputTrigger::ButtonReleased)
    {
        m_forward_value = 0;
    }
}

void ExampleController::HandleMoveRight(Rndr::InputPrimitive, Rndr::InputTrigger trigger, Rndr::f32 value, bool)
{
    if (trigger == Rndr::InputTrigger::ButtonPressed)
    {
        m_right_value = m_move_speed * value;
    }
    else if (trigger == Rndr::InputTrigger::ButtonReleased)
    {
        m_right_value = 0;
    }
}

#else  // !RNDR_OLD_INPUT_SYSTEM

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

#endif  // RNDR_OLD_INPUT_SYSTEM