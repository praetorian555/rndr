#include "rndr/camera/firstpersoncamera.h"

#include "rndr/core/projectioncamera.h"
#include "rndr/core/log.h"
#include "rndr/core/rndrapp.h"
#include "rndr/core/math.h"

rndr::FirstPersonCamera::FirstPersonCamera(rndr::ProjectionCamera* ProjectionCamera,
                                           rndr::Point3r StartingPosition,
                                           real MovementSpeed,
                                           real RotationSpeed)
    : m_ProjectionCamera(ProjectionCamera), m_Position(StartingPosition), m_MovementSpeed(MovementSpeed), m_RotationSpeed(RotationSpeed)
{
    rndr::InputContext* Context = GRndrApp->GetInputContext();
    assert(Context);

    using IP = rndr::InputPrimitive;
    using IT = rndr::InputTrigger;

    Context->CreateMapping("MoveForward", RNDR_BIND_INPUT_CALLBACK(&FirstPersonCamera::HandleMoveForward, this));
    Context->AddBinding("MoveForward", IP::Keyboard_W, IT::ButtonDown);
    Context->AddBinding("MoveForward", IP::Keyboard_W, IT::ButtonUp);
    Context->AddBinding("MoveForward", IP::Keyboard_S, IT::ButtonDown, -1);
    Context->AddBinding("MoveForward", IP::Keyboard_S, IT::ButtonUp, -1);

    Context->CreateMapping("MoveRight", RNDR_BIND_INPUT_CALLBACK(&FirstPersonCamera::HandleMoveRight, this));
    Context->AddBinding("MoveRight", IP::Keyboard_A, IT::ButtonDown, -1);
    Context->AddBinding("MoveRight", IP::Keyboard_A, IT::ButtonUp, -1);
    Context->AddBinding("MoveRight", IP::Keyboard_D, IT::ButtonDown);
    Context->AddBinding("MoveRight", IP::Keyboard_D, IT::ButtonUp);

    Context->CreateMapping("LookAroundVert", RNDR_BIND_INPUT_CALLBACK(&FirstPersonCamera::HandleLookVert, this));
    Context->AddBinding("LookAroundVert", IP::Keyboard_Up, IT::ButtonDown);
    Context->AddBinding("LookAroundVert", IP::Keyboard_Up, IT::ButtonUp);
    Context->AddBinding("LookAroundVert", IP::Keyboard_Down, IT::ButtonDown, -1);
    Context->AddBinding("LookAroundVert", IP::Keyboard_Down, IT::ButtonUp, -1);

    Context->CreateMapping("LookAroundHorz", RNDR_BIND_INPUT_CALLBACK(&FirstPersonCamera::HandleLookHorz, this));
    Context->AddBinding("LookAroundHorz", IP::Keyboard_Right, IT::ButtonDown, -1);
    Context->AddBinding("LookAroundHorz", IP::Keyboard_Right, IT::ButtonUp, -1);
    Context->AddBinding("LookAroundHorz", IP::Keyboard_Left, IT::ButtonDown, 1);
    Context->AddBinding("LookAroundHorz", IP::Keyboard_Left, IT::ButtonUp, 1);
}

void rndr::FirstPersonCamera::Update(real DeltaSeconds)
{
    m_DirectionVector = Vector3r{0, 0, -1};
    m_DirectionAngles += m_DeltaAngles * m_RotationSpeed;
    m_DirectionVector = math::Rotate(m_DirectionAngles)(m_DirectionVector);
    m_RightVector = math::Cross(m_DirectionVector, Vector3r{0, 1, 0});

    m_Position += m_MovementSpeed * DeltaSeconds * m_DeltaPosition.X * m_DirectionVector;
    m_Position += m_MovementSpeed * DeltaSeconds * m_DeltaPosition.Y * m_RightVector;

    math::Rotator R = m_DirectionAngles;

    const math::Transform CameraToWorld = math::Translate((rndr::Vector3r)m_Position) * math::Rotate(R);
    const math::Transform WorldToCamera(CameraToWorld.GetInverse());

    m_ProjectionCamera->SetWorldToCamera(WorldToCamera);
}

rndr::ProjectionCamera* rndr::FirstPersonCamera::GetProjectionCamera()
{
    return m_ProjectionCamera;
}

void rndr::FirstPersonCamera::SetProjectionCamera(rndr::ProjectionCamera* ProjectionCamera)
{
    m_ProjectionCamera = ProjectionCamera;
}

rndr::Point3r rndr::FirstPersonCamera::GetPosition() const
{
    return m_Position;
}

void rndr::FirstPersonCamera::HandleLookVert(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real AxisValue)
{
    using IT = rndr::InputTrigger;
    m_DeltaAngles.Roll = Trigger == IT::ButtonDown ? m_RotationSpeed * AxisValue : 0;
}

void rndr::FirstPersonCamera::HandleLookHorz(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real AxisValue)
{
    using IT = rndr::InputTrigger;
    m_DeltaAngles.Yaw = Trigger == IT::ButtonDown ? m_RotationSpeed * AxisValue : 0;
}

void rndr::FirstPersonCamera::HandleMoveForward(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real Value)
{
    using IT = rndr::InputTrigger;
    m_DeltaPosition.X = Trigger == IT::ButtonDown ? Value : 0;
}

void rndr::FirstPersonCamera::HandleMoveRight(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real Value)
{
    using IT = rndr::InputTrigger;
    m_DeltaPosition.Y = Trigger == IT::ButtonDown ? Value : 0;
}
