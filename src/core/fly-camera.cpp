#include "rndr/core/fly-camera.h"

#include "math/transform.h"

#include "opal/container/array.h"

#include "rndr/core/types.h"
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

    Opal::Array<InputBinding> activate_bindings;
    activate_bindings.PushBack(InputBinding{.primitive = IP::Mouse_RightButton, .trigger = IT::ButtonPressed});
    activate_bindings.PushBack(InputBinding{.primitive = IP::Mouse_RightButton, .trigger = IT::ButtonReleased});
    m_input_context->AddAction(
        InputAction(u8"ActivateCamera"),
        InputActionData{.callback = RNDR_BIND_INPUT_CALLBACK(this, FlyCamera::HandleActivate), .bindings = activate_bindings});

    Opal::Array<InputBinding> forward_bindings;
    forward_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_W, .trigger = IT::ButtonPressed, .modifier = -1});
    forward_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_W, .trigger = IT::ButtonReleased});
    forward_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_S, .trigger = IT::ButtonPressed});
    forward_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_S, .trigger = IT::ButtonReleased});
    m_input_context->AddAction(
        InputAction(u8"MoveForward"),
        InputActionData{.callback = RNDR_BIND_INPUT_CALLBACK(this, FlyCamera::HandleMoveForward), .bindings = forward_bindings});

    Opal::Array<InputBinding> right_bindings;
    right_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_A, .trigger = IT::ButtonPressed});
    right_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_A, .trigger = IT::ButtonReleased});
    right_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_D, .trigger = IT::ButtonPressed, .modifier = -1});
    right_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_D, .trigger = IT::ButtonReleased});
    m_input_context->AddAction(
        InputAction(u8"MoveRight"),
        InputActionData{.callback = RNDR_BIND_INPUT_CALLBACK(this, FlyCamera::HandleMoveRight), .bindings = right_bindings});

    Opal::Array<InputBinding> vert_bindings;
    vert_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_UpArrow, .trigger = IT::ButtonPressed});
    vert_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_UpArrow, .trigger = IT::ButtonReleased});
    vert_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_DownArrow, .trigger = IT::ButtonPressed, .modifier = -1});
    vert_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_DownArrow, .trigger = IT::ButtonReleased, .modifier = -1});
    vert_bindings.PushBack(InputBinding{.primitive = IP::Mouse_AxisY, .trigger = IT::AxisChangedRelative, .modifier = -1});
    m_input_context->AddAction(
        InputAction(u8"LookAroundVert"),
        InputActionData{.callback = RNDR_BIND_INPUT_CALLBACK(this, FlyCamera::HandleLookVert), .bindings = vert_bindings});

    Opal::Array<InputBinding> horz_bindings;
    horz_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_RightArrow, .trigger = IT::ButtonPressed, .modifier = -1});
    horz_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_RightArrow, .trigger = IT::ButtonReleased, .modifier = -1});
    horz_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_LeftArrow, .trigger = IT::ButtonPressed});
    horz_bindings.PushBack(InputBinding{.primitive = IP::Keyboard_LeftArrow, .trigger = IT::ButtonReleased});
    horz_bindings.PushBack(InputBinding{.primitive = IP::Mouse_AxisX, .trigger = IT::AxisChangedRelative, .modifier = -1});
    m_input_context->AddAction(
        InputAction(u8"LookAroundHorz"),
        InputActionData{.callback = RNDR_BIND_INPUT_CALLBACK(this, FlyCamera::HandleLookHorz), .bindings = horz_bindings});
}

Rndr::FlyCamera::~FlyCamera()
{
    m_window->on_resize.Unbind(m_window_resize_handle);
}

void Rndr::FlyCamera::Update(f32 delta_seconds)
{
    if (!m_is_active)
    {
        return;
    }

    m_direction_vector = Vector3f{0, 0, 1};  // Left-handed
    Rotatorf rotation = GetRotation();
    rotation += delta_seconds * m_desc.rotation_speed * m_delta_rotation;
    m_direction_vector = Math::Rotate(rotation) * m_direction_vector;
    m_right_vector = Math::Cross(m_direction_vector, Vector3f{0, 1, 0});

    constexpr f32 k_max_roll = 89.0f;
    rotation.roll = Math::Clamp(rotation.roll, -k_max_roll, k_max_roll);

    Point3f position = GetPosition();
    position += m_desc.movement_speed * delta_seconds * m_delta_position.x * m_direction_vector;
    position += m_desc.movement_speed * delta_seconds * m_delta_position.y * m_right_vector;

    SetPositionAndRotation(position, rotation);

    m_delta_rotation = Rotatorf::Zero();
}

void Rndr::FlyCamera::HandleActivate(InputPrimitive primitive, InputTrigger trigger, f32 value)
{
    RNDR_UNUSED(primitive);
    RNDR_UNUSED(value);
    using IT = Rndr::InputTrigger;
    const bool new_is_active = trigger == IT::ButtonPressed;
    if (new_is_active != m_is_active)
    {
        if (new_is_active)
        {
            m_prev_cursor_mode = m_window->GetCursorMode();
            m_window->SetCursorMode(CursorMode::Infinite);
        }
        else
        {
            m_window->SetCursorMode(m_prev_cursor_mode);
        }
        m_is_active = new_is_active;
    }
}

void Rndr::FlyCamera::HandleLookVert(Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, f32 axis_value)
{
    using IT = Rndr::InputTrigger;
    if (primitive == Rndr::InputPrimitive::Mouse_AxisY)
    {
        m_delta_rotation.roll = m_desc.rotation_speed * axis_value;
    }
    else
    {
        m_delta_rotation.roll = trigger == IT::ButtonPressed ? m_desc.rotation_speed * axis_value : 0;
    }
}

void Rndr::FlyCamera::HandleLookHorz(Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, f32 axis_value)
{
    using IT = Rndr::InputTrigger;
    if (primitive == Rndr::InputPrimitive::Mouse_AxisX)
    {
        m_delta_rotation.yaw = m_desc.rotation_speed * axis_value;
    }
    else
    {
        m_delta_rotation.yaw = trigger == IT::ButtonPressed ? m_desc.rotation_speed * axis_value : 0;
    }
}

void Rndr::FlyCamera::HandleMoveForward(Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, f32 value)
{
    RNDR_UNUSED(primitive);
    using IT = Rndr::InputTrigger;
    m_delta_position.x = trigger == IT::ButtonPressed ? value : 0;
}

void Rndr::FlyCamera::HandleMoveRight(Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, f32 value)
{
    RNDR_UNUSED(primitive);
    using IT = Rndr::InputTrigger;
    m_delta_position.y = trigger == IT::ButtonPressed ? value : 0;
}
