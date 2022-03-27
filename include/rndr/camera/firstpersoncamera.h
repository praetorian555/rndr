#pragma once

#include "rndr/core/base.h"
#include "rndr/core/input.h"
#include "rndr/core/math.h"

namespace rndr
{

class Camera;

/**
 * Wrapper for movement of the camera from the first person point of view. Used to acquire world to
 * camera transform for projection cameras.
 */
class FirstPersonCamera
{
public:
    FirstPersonCamera(rndr::Camera* ProjectionCamera,
                      real MovementSpeed = 1.0,
                      real RotationSpeed = 1.0);

    void Update(real DeltaSeconds);

    rndr::Camera* GetProjectionCamera();
    void SetProjectionCamera(rndr::Camera* ProjectionCamera);

private:
    void HandleLookVert(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real AxisValue);
    void HandleLookHorz(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real AxisValue);
    void HandleMoveForward(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real);
    void HandleMoveRight(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real);

private:
    rndr::Camera* m_ProjectionCamera = nullptr;

    Vector3r m_Position;
    Rotator m_DirectionAngles;

    Rotator m_DeltaAngles;
    Vector3r m_DeltaPosition;

    Vector3r m_DirectionVector;
    Vector3r m_RightVector;

    real m_MovementSpeed;
    real m_RotationSpeed;

    bool m_bDirectionChanged = false;
};

}  // namespace rndr