#include "rndr/fly-camera.h"

#include "opal/container/dynamic-array.h"
#include "opal/math/transform.h"

#include "rndr/types.h"
#include "rndr/window.h"

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

    Opal::DynamicArray<InputBinding> activate_bindings;
    activate_bindings.PushBack(InputBinding::CreateMouseButtonBinding(
        IP::Mouse_RightButton, IT::ButtonPressed, RNDR_BIND_INPUT_MOUSE_BUTTON_CALLBACK(this, FlyCamera::HandleActivate)));
    activate_bindings.PushBack(InputBinding::CreateMouseButtonBinding(
        IP::Mouse_RightButton, IT::ButtonReleased, RNDR_BIND_INPUT_MOUSE_BUTTON_CALLBACK(this, FlyCamera::HandleActivate)));
    m_input_context->AddAction("ActivateCamera", activate_bindings);

    Opal::DynamicArray<InputBinding> forward_bindings;
    forward_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::W, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleMoveForward)));
    forward_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::W, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleMoveForward)));
    forward_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::S, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleMoveForward), -1));
    forward_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::S, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleMoveForward)));
    m_input_context->AddAction("MoveForward", forward_bindings);

    Opal::DynamicArray<InputBinding> right_bindings;
    right_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::A, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleMoveRight), -1));
    right_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(IP::A, IT::ButtonReleased,
                                                                      RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleMoveRight)));
    right_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(IP::D, IT::ButtonPressed,
                                                                      RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleMoveRight)));
    right_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(IP::D, IT::ButtonReleased,
                                                                      RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleMoveRight)));
    m_input_context->AddAction("MoveRight", right_bindings);

    Opal::DynamicArray<InputBinding> vert_bindings;
    vert_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::UpArrow, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleLookVerticalButton)));
    vert_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::UpArrow, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleLookVerticalButton)));
    vert_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::DownArrow, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleLookVerticalButton)));
    vert_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::DownArrow, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleLookVerticalButton)));
    vert_bindings.PushBack(InputBinding::CreateMousePositionBinding(
        IP::Mouse_AxisY, RNDR_BIND_INPUT_MOUSE_POSITION_CALLBACK(this, FlyCamera::HandleLookVertical)));
    m_input_context->AddAction("LookAroundVert", vert_bindings);

    Opal::DynamicArray<InputBinding> horz_bindings;
    horz_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::RightArrow, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleLookHorizontalButton)));
    horz_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::RightArrow, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleLookHorizontalButton)));
    horz_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::LeftArrow, IT::ButtonPressed, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleLookHorizontalButton)));
    horz_bindings.PushBack(InputBinding::CreateKeyboardButtonBinding(
        IP::LeftArrow, IT::ButtonReleased, RNDR_BIND_INPUT_BUTTON_CALLBACK(this, FlyCamera::HandleLookHorizontalButton)));
    horz_bindings.PushBack(InputBinding::CreateMousePositionBinding(
        IP::Mouse_AxisX, RNDR_BIND_INPUT_MOUSE_POSITION_CALLBACK(this, FlyCamera::HandleLookHorizontal)));
    m_input_context->AddAction("LookAroundHorz", horz_bindings);
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

    m_direction_vector = Vector3f{0, 0, -1};
    Rotatorf rotation = GetRotation();
    rotation += delta_seconds * m_desc.rotation_speed * m_delta_rotation;
    m_direction_vector = Opal::Rotate(rotation) * m_direction_vector;
    m_right_vector = Opal::Cross(m_direction_vector, Vector3f{0, 1, 0});

    constexpr f32 k_max_roll = 89.0f;
    rotation.roll = Opal::Clamp(rotation.roll, -k_max_roll, k_max_roll);

    Point3f position = GetPosition();
    position += m_desc.movement_speed * delta_seconds * m_delta_position.x * m_direction_vector;
    position += m_desc.movement_speed * delta_seconds * m_delta_position.y * m_right_vector;

    SetPositionAndRotation(position, rotation);

    m_delta_rotation = Rotatorf::Zero();
}

void Rndr::FlyCamera::HandleActivate(InputPrimitive primitive, InputTrigger trigger, f32 value, const Vector2i&)
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

void Rndr::FlyCamera::HandleLookVertical(InputPrimitive, f32 axis_value)
{
    m_delta_rotation.roll = -m_desc.rotation_speed * axis_value;
}

void Rndr::FlyCamera::HandleLookVerticalButton(InputPrimitive, InputTrigger trigger, f32 value, bool)
{
    m_delta_rotation.roll = trigger == InputTrigger::ButtonPressed ? m_desc.rotation_speed * value : 0;
}

void Rndr::FlyCamera::HandleLookHorizontal(InputPrimitive, f32 axis_value)
{
    m_delta_rotation.yaw = -m_desc.rotation_speed * axis_value;
}

void Rndr::FlyCamera::HandleLookHorizontalButton(InputPrimitive, InputTrigger trigger, f32 value, bool)
{
    m_delta_rotation.yaw = trigger == InputTrigger::ButtonPressed ? m_desc.rotation_speed * value : 0;
}

void Rndr::FlyCamera::HandleMoveForward(Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, f32 value, bool)
{
    RNDR_UNUSED(primitive);
    using IT = Rndr::InputTrigger;
    m_delta_position.x = trigger == IT::ButtonPressed ? value : 0;
}

void Rndr::FlyCamera::HandleMoveRight(Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, f32 value, bool)
{
    RNDR_UNUSED(primitive);
    using IT = Rndr::InputTrigger;
    m_delta_position.y = trigger == IT::ButtonPressed ? value : 0;
}
