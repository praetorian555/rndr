#pragma once

#include "math/point3.h"
#include "math/rotator.h"
#include "math/vector3.h"

#include "rndr/core/base.h"
#include "rndr/core/input.h"
#include "rndr/core/rndrcontext.h"

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
    explicit FirstPersonCamera(RndrContext* RndrContext,
                               ProjectionCamera* ProjectionCamera,
                               math::Point3 StartingPosition = math::Point3(),
                               real MovementSpeed = 1.0,
                               real RotationSpeed = 1.0);

    void Update(real DeltaSeconds);

    ProjectionCamera* GetProjectionCamera();
    void SetProjectionCamera(ProjectionCamera* ProjectionCamera);

    math::Point3 GetPosition() const;

private:
    void HandleLookVert(InputPrimitive Primitive, InputTrigger Trigger, real AxisValue);
    void HandleLookHorz(InputPrimitive Primitive, InputTrigger Trigger, real AxisValue);
    void HandleMoveForward(InputPrimitive Primitive, InputTrigger Trigger, real);
    void HandleMoveRight(InputPrimitive Primitive, InputTrigger Trigger, real);

private:
    ProjectionCamera* m_ProjectionCamera = nullptr;

    math::Point3 m_Position;
    math::Rotator m_DirectionAngles;

    math::Rotator m_DeltaAngles;
    math::Vector3 m_DeltaPosition;

    math::Vector3 m_DirectionVector;
    math::Vector3 m_RightVector;

    real m_MovementSpeed;
    real m_RotationSpeed;

    bool m_bDirectionChanged = false;
};

}  // namespace rndr