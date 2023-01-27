#include "rndr/utility/flycamera.h"

#include "math/transform.h"

#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/projectioncamera.h"

rndr::FlyCamera::FlyCamera(InputContext* InputContext,
                           int ScreenWidth,
                           int ScreenHeight,
                           const FlyCameraProperties& Props)
    : ProjectionCamera({}, ScreenWidth, ScreenHeight, Props.ProjectionProps),
      m_InputContext(InputContext),
      m_Props(Props),
      m_DirectionAngles(Props.StartRotation)
{
    m_Position += Props.StartPosition;

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

    m_InputContext->CreateMapping("LookAroundHorz",
                                  RNDR_BIND_INPUT_CALLBACK(&FlyCamera::HandleLookHorz, this));
    m_InputContext->AddBinding("LookAroundHorz", IP::Keyboard_Right, IT::ButtonDown, -1);
    m_InputContext->AddBinding("LookAroundHorz", IP::Keyboard_Right, IT::ButtonUp, -1);
    m_InputContext->AddBinding("LookAroundHorz", IP::Keyboard_Left, IT::ButtonDown, 1);
    m_InputContext->AddBinding("LookAroundHorz", IP::Keyboard_Left, IT::ButtonUp, 1);
}

void rndr::FlyCamera::Update(real DeltaSeconds)
{
#if defined RNDR_LEFT_HANDED
    const real HandednessMultiplier = -1;
#else
    const real HandednessMultiplier = 1;
#endif

    m_DirectionVector = math::Vector3{0, 0, -1};
    m_DirectionAngles += HandednessMultiplier * m_DeltaAngles * m_Props.RotationSpeed;
    m_DirectionVector = math::Rotate(m_DirectionAngles)(m_DirectionVector);
    m_RightVector = math::Cross(m_DirectionVector, math::Vector3{0, 1, 0});

    m_Position += HandednessMultiplier * m_Props.MovementSpeed * DeltaSeconds * m_DeltaPosition.X *
                  m_DirectionVector;
    m_Position += m_Props.MovementSpeed * DeltaSeconds * m_DeltaPosition.Y * m_RightVector;

    const math::Transform CameraToWorld =
        math::Translate(math::Vector3{m_Position}) * math::Rotate(math::Rotator(m_DirectionAngles));
    const math::Transform WorldToCamera(CameraToWorld.GetInverse());

    SetWorldToCamera(WorldToCamera);

    m_DeltaAngles = math::Rotator{};
}

math::Point3 rndr::FlyCamera::GetPosition() const
{
    return m_Position;
}

void rndr::FlyCamera::HandleLookVert(rndr::InputPrimitive Primitive,
                                     rndr::InputTrigger Trigger,
                                     real AxisValue)
{
    RNDR_UNUSED(Primitive);
    using IT = rndr::InputTrigger;
    m_DeltaAngles.Roll = Trigger == IT::ButtonDown ? m_Props.RotationSpeed * AxisValue : 0;
}

void rndr::FlyCamera::HandleLookHorz(rndr::InputPrimitive Primitive,
                                     rndr::InputTrigger Trigger,
                                     real AxisValue)
{
    RNDR_UNUSED(Primitive);
    using IT = rndr::InputTrigger;
    m_DeltaAngles.Yaw = Trigger == IT::ButtonDown ? m_Props.RotationSpeed * AxisValue : 0;
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
