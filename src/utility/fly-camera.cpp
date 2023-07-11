#include "rndr/utility/fly-camera.h"

#include "math/transform.h"

#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/projection-camera.h"

rndr::FlyCamera::FlyCamera(InputContext* InputContext,
                           int ScreenWidth,
                           int ScreenHeight,
                           const FlyCameraProperties& Props)
    : ProjectionCamera(Props.StartPosition,
                       Props.StartRotation,
                       ScreenWidth,
                       ScreenHeight,
                       Props.ProjectionProps),
      m_InputContext(InputContext),
      m_Props(Props)
{
    using IP = rndr::InputPrimitive;
    using IT = rndr::InputTrigger;

    m_InputContext->CreateMapping("MoveForward",
                                  RNDR_BIND_INPUT_CALLBACK(&FlyCamera::HandleMoveForward, this));
    m_InputContext->AddBinding("MoveForward", IP::Keyboard_W, IT::ButtonDown);
    m_InputContext->AddBinding("MoveForward", IP::Keyboard_W, IT::ButtonUp);
    m_InputContext->AddBinding("MoveForward", IP::Keyboard_S, IT::ButtonDown, -1);
    m_InputContext->AddBinding("MoveForward", IP::Keyboard_S, IT::ButtonUp, -1);

    m_InputContext->CreateMapping("MoveRight",
                                  RNDR_BIND_INPUT_CALLBACK(&FlyCamera::HandleMoveRight, this));
    m_InputContext->AddBinding("MoveRight", IP::Keyboard_A, IT::ButtonDown, -1);
    m_InputContext->AddBinding("MoveRight", IP::Keyboard_A, IT::ButtonUp, -1);
    m_InputContext->AddBinding("MoveRight", IP::Keyboard_D, IT::ButtonDown);
    m_InputContext->AddBinding("MoveRight", IP::Keyboard_D, IT::ButtonUp);

    m_InputContext->CreateMapping("LookAroundVert",
                                  RNDR_BIND_INPUT_CALLBACK(&FlyCamera::HandleLookVert, this));
    m_InputContext->AddBinding("LookAroundVert", IP::Keyboard_Up, IT::ButtonDown);
    m_InputContext->AddBinding("LookAroundVert", IP::Keyboard_Up, IT::ButtonUp);
    m_InputContext->AddBinding("LookAroundVert", IP::Keyboard_Down, IT::ButtonDown, -1);
    m_InputContext->AddBinding("LookAroundVert", IP::Keyboard_Down, IT::ButtonUp, -1);
    m_InputContext->AddBinding("LookAroundVert", IP::Mouse_AxisY, IT::AxisChangedRelative, -1);

    m_InputContext->CreateMapping("LookAroundHorz",
                                  RNDR_BIND_INPUT_CALLBACK(&FlyCamera::HandleLookHorz, this));
    m_InputContext->AddBinding("LookAroundHorz", IP::Keyboard_Right, IT::ButtonDown, -1);
    m_InputContext->AddBinding("LookAroundHorz", IP::Keyboard_Right, IT::ButtonUp, -1);
    m_InputContext->AddBinding("LookAroundHorz", IP::Keyboard_Left, IT::ButtonDown, 1);
    m_InputContext->AddBinding("LookAroundHorz", IP::Keyboard_Left, IT::ButtonUp, 1);
    m_InputContext->AddBinding("LookAroundHorz", IP::Mouse_AxisX, IT::AxisChangedRelative, -1);
}

void rndr::FlyCamera::Update(real DeltaSeconds)
{
    m_DirectionVector = math::Vector3{0, 0, 1};  // Left-handed
    math::Rotator Rotation = GetRotation();
    Rotation += DeltaSeconds * m_Props.RotationSpeed * m_DeltaRotation;
    m_DirectionVector = math::Rotate(Rotation)(m_DirectionVector);
    m_RightVector = math::Cross(m_DirectionVector, math::Vector3{0, 1, 0});

    constexpr real kMaxRoll = 89.0f;
    Rotation.Roll = math::Clamp(Rotation.Roll, -kMaxRoll, kMaxRoll);

    math::Point3 Position = GetPosition();
    Position += m_Props.MovementSpeed * DeltaSeconds * m_DeltaPosition.X * m_DirectionVector;
    Position += m_Props.MovementSpeed * DeltaSeconds * m_DeltaPosition.Y * m_RightVector;

    SetPositionAndRotation(Position, Rotation);

    m_DeltaRotation = math::Rotator{};
}

void rndr::FlyCamera::HandleLookVert(rndr::InputPrimitive Primitive,
                                     rndr::InputTrigger Trigger,
                                     real AxisValue)
{
    using IT = rndr::InputTrigger;
    if (Primitive == rndr::InputPrimitive::Mouse_AxisY)
    {
        m_DeltaRotation.Roll = m_Props.RotationSpeed * AxisValue;
    }
    else
    {
        m_DeltaRotation.Roll = Trigger == IT::ButtonDown ? m_Props.RotationSpeed * AxisValue : 0;
    }
}

void rndr::FlyCamera::HandleLookHorz(rndr::InputPrimitive Primitive,
                                     rndr::InputTrigger Trigger,
                                     real AxisValue)
{
    using IT = rndr::InputTrigger;
    if (Primitive == rndr::InputPrimitive::Mouse_AxisX)
    {
        m_DeltaRotation.Yaw = m_Props.RotationSpeed * AxisValue;
    }
    else
    {
        m_DeltaRotation.Yaw = Trigger == IT::ButtonDown ? m_Props.RotationSpeed * AxisValue : 0;
    }
}

void rndr::FlyCamera::HandleMoveForward(rndr::InputPrimitive Primitive,
                                        rndr::InputTrigger Trigger,
                                        real Value)
{
    RNDR_UNUSED(Primitive);
    using IT = rndr::InputTrigger;
    m_DeltaPosition.X = Trigger == IT::ButtonDown ? Value : 0;
}

void rndr::FlyCamera::HandleMoveRight(rndr::InputPrimitive Primitive,
                                      rndr::InputTrigger Trigger,
                                      real Value)
{
    RNDR_UNUSED(Primitive);
    using IT = rndr::InputTrigger;
    m_DeltaPosition.Y = Trigger == IT::ButtonDown ? Value : 0;
}
