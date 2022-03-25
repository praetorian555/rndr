#include "rndr/camera/firstpersoncamera.h"

#include "rndr/core/camera.h"
#include "rndr/core/log.h"
#include "rndr/core/rndrapp.h"
#include "rndr/core/transform.h"

rndr::FirstPersonCamera::FirstPersonCamera(rndr::Camera* ProjectionCamera)
    : m_ProjectionCamera(ProjectionCamera)
{
    rndr::InputContext* Context = GRndrApp->GetInputContext();
    assert(Context);

    using IP = rndr::InputPrimitive;
    using IT = rndr::InputTrigger;

    Context->CreateMapping("MoveForward",
                           RNDR_BIND_INPUT_CALLBACK(&FirstPersonCamera::HandleMoveForward, this));
    Context->AddBinding("MoveForward", IP::Keyboard_W, IT::ButtonDown);
    Context->AddBinding("MoveForward", IP::Keyboard_W, IT::ButtonUp);
    Context->AddBinding("MoveForward", IP::Keyboard_S, IT::ButtonDown, -1);
    Context->AddBinding("MoveForward", IP::Keyboard_S, IT::ButtonUp, -1);

    Context->CreateMapping("MoveRight",
                           RNDR_BIND_INPUT_CALLBACK(&FirstPersonCamera::HandleMoveRight, this));
    Context->AddBinding("MoveRight", IP::Keyboard_A, IT::ButtonDown, -1);
    Context->AddBinding("MoveRight", IP::Keyboard_A, IT::ButtonUp, -1);
    Context->AddBinding("MoveRight", IP::Keyboard_D, IT::ButtonDown);
    Context->AddBinding("MoveRight", IP::Keyboard_D, IT::ButtonUp);

    Context->CreateMapping("LookAroundVert",
                           RNDR_BIND_INPUT_CALLBACK(&FirstPersonCamera::HandleLookVert, this));
    Context->AddBinding("LookAroundVert", IP::Keyboard_Up, IT::ButtonDown);
    Context->AddBinding("LookAroundVert", IP::Keyboard_Up, IT::ButtonUp);
    Context->AddBinding("LookAroundVert", IP::Keyboard_Down, IT::ButtonDown, -1);
    Context->AddBinding("LookAroundVert", IP::Keyboard_Down, IT::ButtonUp, -1);

    Context->CreateMapping("LookAroundHorz",
                           RNDR_BIND_INPUT_CALLBACK(&FirstPersonCamera::HandleLookHorz, this));
    Context->AddBinding("LookAroundHorz", IP::Keyboard_Right, IT::ButtonDown, -1);
    Context->AddBinding("LookAroundHorz", IP::Keyboard_Right, IT::ButtonUp, -1);
    Context->AddBinding("LookAroundHorz", IP::Keyboard_Left, IT::ButtonDown, 1);
    Context->AddBinding("LookAroundHorz", IP::Keyboard_Left, IT::ButtonUp, 1);
}

void rndr::FirstPersonCamera::Update(real DeltaSeconds)
{
    m_DirectionVector = Vector3r{0, 0, -1};
    m_DirectionAngles += m_DeltaAngles;
    m_DirectionVector = rndr::Rotate(m_DirectionAngles)(m_DirectionVector);
    m_RightVector = rndr::Cross(m_DirectionVector, Vector3r{0, 1, 0});

    const real Speed = 5;
    m_Position += Speed * DeltaSeconds * m_DeltaPosition.X * m_DirectionVector;
    m_Position += Speed * DeltaSeconds * m_DeltaPosition.Y * m_RightVector;

    Rotator R = m_DirectionAngles;

    const Transform CameraToWorld = rndr::Translate(m_Position) * rndr::Rotate(R);
    const Transform WorldToCamera = CameraToWorld.GetInverse();

    m_ProjectionCamera->UpdateWorldToCamera(WorldToCamera);
}

rndr::Camera* rndr::FirstPersonCamera::GetProjectionCamera()
{
    return m_ProjectionCamera;
}

void rndr::FirstPersonCamera::SetProjectionCamera(rndr::Camera* ProjectionCamera)
{
    m_ProjectionCamera = ProjectionCamera;
}

void rndr::FirstPersonCamera::HandleLookVert(rndr::InputPrimitive Primitive,
                                             rndr::InputTrigger Trigger,
                                             real AxisValue)
{
    using IP = rndr::InputPrimitive;
    using IT = rndr::InputTrigger;

    if (Trigger == IT::ButtonDown)
    {
        m_DeltaAngles.Roll = AxisValue;
    }
    else
    {
        m_DeltaAngles.Roll = 0;
    }
}

void rndr::FirstPersonCamera::HandleLookHorz(rndr::InputPrimitive Primitive,
                                             rndr::InputTrigger Trigger,
                                             real AxisValue)
{
    using IP = rndr::InputPrimitive;
    using IT = rndr::InputTrigger;

    if (Trigger == IT::ButtonDown)
    {
        m_DeltaAngles.Yaw = AxisValue;
    }
    else
    {
        m_DeltaAngles.Yaw = 0;
    }
}

void rndr::FirstPersonCamera::HandleMoveForward(rndr::InputPrimitive Primitive,
                                                rndr::InputTrigger Trigger,
                                                real Value)
{
    using IP = rndr::InputPrimitive;
    using IT = rndr::InputTrigger;

    if (Trigger == IT::ButtonDown)
    {
        m_DeltaPosition.X = Value;
    }
    else
    {
        m_DeltaPosition.X = 0;
    }
}

void rndr::FirstPersonCamera::HandleMoveRight(rndr::InputPrimitive Primitive,
                                              rndr::InputTrigger Trigger,
                                              real Value)
{
    using IP = rndr::InputPrimitive;
    using IT = rndr::InputTrigger;

    if (Trigger == IT::ButtonDown)
    {
        m_DeltaPosition.Y = Value;
    }
    else
    {
        m_DeltaPosition.Y = 0;
    }
}