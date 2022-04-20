#pragma once

#include "rndr/core/base.h"
#include "rndr/core/input.h"
#include "rndr/core/math.h"

namespace rndr
{

class ProjectionCamera;

/**
 * Wrapper for movement of the camera from the first person point of view. Used to acquire world to
 * camera transform for projection cameras.
 */
class FirstPersonCamera
{
public:
    FirstPersonCamera(ProjectionCamera* ProjectionCamera,
                      Point3r StartingPosition = Point3r(),
                      real MovementSpeed = 1.0,
                      real RotationSpeed = 1.0);

    void Update(real DeltaSeconds);

    ProjectionCamera* GetProjectionCamera();
    void SetProjectionCamera(ProjectionCamera* ProjectionCamera);

    Point3r GetPosition() const;

private:
    void HandleLookVert(InputPrimitive Primitive, InputTrigger Trigger, real AxisValue);
    void HandleLookHorz(InputPrimitive Primitive, InputTrigger Trigger, real AxisValue);
    void HandleMoveForward(InputPrimitive Primitive, InputTrigger Trigger, real);
    void HandleMoveRight(InputPrimitive Primitive, InputTrigger Trigger, real);

private:
    ProjectionCamera* m_ProjectionCamera = nullptr;

    Point3r m_Position;
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