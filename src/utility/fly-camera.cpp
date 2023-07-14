#include "Rndr/utility/fly-camera.h"

#include "math/transform.h"

#include "rndr/core/window.h"

Rndr::FlyCamera::FlyCamera(Window* window, InputContext* input_context, const FlyCameraDesc& desc)
    : ProjectionCamera(desc.start_position, desc.start_rotation, window->GetWidth(), window->GetHeight(), desc.projection_desc),
      m_window(window),
      m_input_context(input_context),
      m_desc(desc),
      m_prev_cursor_mode(m_window->GetCursorMode())
{
    using IP = Rndr::InputPrimitive;
    using IT = Rndr::InputTrigger;

    m_window_resize_handle = m_window->on_resize.Bind([this](int width, int height) { this->SetScreenSize(width, height); });
    m_window->SetCursorMode(CursorMode::Infinite);

    Array<InputBinding> forward_bindings;
    forward_bindings.push_back(InputBinding{.primitive = IP::Keyboard_W, .trigger = IT::ButtonPressed, .modifier = -1});
    forward_bindings.push_back(InputBinding{.primitive = IP::Keyboard_W, .trigger = IT::ButtonReleased});
    forward_bindings.push_back(InputBinding{.primitive = IP::Keyboard_S, .trigger = IT::ButtonPressed});
    forward_bindings.push_back(InputBinding{.primitive = IP::Keyboard_S, .trigger = IT::ButtonReleased});
    m_input_context->AddAction(
        InputAction("MoveForward"),
        InputActionData{.callback = RNDR_BIND_INPUT_CALLBACK(this, FlyCamera::HandleMoveForward), .bindings = forward_bindings});

    Array<InputBinding> right_bindings;
    right_bindings.push_back(InputBinding{.primitive = IP::Keyboard_A, .trigger = IT::ButtonPressed});
    right_bindings.push_back(InputBinding{.primitive = IP::Keyboard_A, .trigger = IT::ButtonReleased});
    right_bindings.push_back(InputBinding{.primitive = IP::Keyboard_D, .trigger = IT::ButtonPressed, .modifier = -1});
    right_bindings.push_back(InputBinding{.primitive = IP::Keyboard_D, .trigger = IT::ButtonReleased});
    m_input_context->AddAction(
        InputAction("MoveRight"),
        InputActionData{.callback = RNDR_BIND_INPUT_CALLBACK(this, FlyCamera::HandleMoveRight), .bindings = right_bindings});

    Array<InputBinding> vert_bindings;
    vert_bindings.push_back(InputBinding{.primitive = IP::Keyboard_UpArrow, .trigger = IT::ButtonPressed});
    vert_bindings.push_back(InputBinding{.primitive = IP::Keyboard_UpArrow, .trigger = IT::ButtonReleased});
    vert_bindings.push_back(InputBinding{.primitive = IP::Keyboard_DownArrow, .trigger = IT::ButtonPressed, .modifier = -1});
    vert_bindings.push_back(InputBinding{.primitive = IP::Keyboard_DownArrow, .trigger = IT::ButtonReleased, .modifier = -1});
    vert_bindings.push_back(InputBinding{.primitive = IP::Mouse_AxisY, .trigger = IT::AxisChangedRelative, .modifier = -1});
    m_input_context->AddAction(
        InputAction("LookAroundVert"),
        InputActionData{.callback = RNDR_BIND_INPUT_CALLBACK(this, FlyCamera::HandleLookVert), .bindings = vert_bindings});

    Array<InputBinding> horz_bindings;
    horz_bindings.push_back(InputBinding{.primitive = IP::Keyboard_RightArrow, .trigger = IT::ButtonPressed, .modifier = -1});
    horz_bindings.push_back(InputBinding{.primitive = IP::Keyboard_RightArrow, .trigger = IT::ButtonReleased, .modifier = -1});
    horz_bindings.push_back(InputBinding{.primitive = IP::Keyboard_LeftArrow, .trigger = IT::ButtonPressed});
    horz_bindings.push_back(InputBinding{.primitive = IP::Keyboard_LeftArrow, .trigger = IT::ButtonReleased});
    horz_bindings.push_back(InputBinding{.primitive = IP::Mouse_AxisX, .trigger = IT::AxisChangedRelative, .modifier = -1});
    m_input_context->AddAction(
        InputAction("LookAroundHorz"),
        InputActionData{.callback = RNDR_BIND_INPUT_CALLBACK(this, FlyCamera::HandleLookHorz), .bindings = horz_bindings});
}

Rndr::FlyCamera::~FlyCamera()
{
    m_window->on_resize.Unbind(m_window_resize_handle);
    m_window->SetCursorMode(m_prev_cursor_mode);
}

void Rndr::FlyCamera::Update(real delta_seconds)
{
    m_direction_vector = math::Vector3{0, 0, 1};  // Left-handed
    math::Rotator rotation = GetRotation();
    rotation += delta_seconds * m_desc.rotation_speed * m_delta_rotation;
    m_direction_vector = math::Rotate(rotation)(m_direction_vector);
    m_right_vector = math::Cross(m_direction_vector, math::Vector3{0, 1, 0});

    constexpr real k_max_roll = 89.0f;
    rotation.Roll = math::Clamp(rotation.Roll, -k_max_roll, k_max_roll);

    math::Point3 position = GetPosition();
    position += m_desc.movement_speed * delta_seconds * m_delta_position.X * m_direction_vector;
    position += m_desc.movement_speed * delta_seconds * m_delta_position.Y * m_right_vector;

    SetPositionAndRotation(position, rotation);

    m_delta_rotation = math::Rotator{};
}

void Rndr::FlyCamera::HandleLookVert(Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, real axis_value)
{
    using IT = Rndr::InputTrigger;
    if (primitive == Rndr::InputPrimitive::Mouse_AxisY)
    {
        m_delta_rotation.Roll = m_desc.rotation_speed * axis_value;
    }
    else
    {
        m_delta_rotation.Roll = trigger == IT::ButtonPressed ? m_desc.rotation_speed * axis_value : 0;
    }
}

void Rndr::FlyCamera::HandleLookHorz(Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, real axis_value)
{
    using IT = Rndr::InputTrigger;
    if (primitive == Rndr::InputPrimitive::Mouse_AxisX)
    {
        m_delta_rotation.Yaw = m_desc.rotation_speed * axis_value;
    }
    else
    {
        m_delta_rotation.Yaw = trigger == IT::ButtonPressed ? m_desc.rotation_speed * axis_value : 0;
    }
}

void Rndr::FlyCamera::HandleMoveForward(Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, real value)
{
    RNDR_UNUSED(primitive);
    using IT = Rndr::InputTrigger;
    m_delta_position.X = trigger == IT::ButtonPressed ? value : 0;
}

void Rndr::FlyCamera::HandleMoveRight(Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, real value)
{
    RNDR_UNUSED(primitive);
    using IT = Rndr::InputTrigger;
    m_delta_position.Y = trigger == IT::ButtonPressed ? value : 0;
}
