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
 * Represents camera in the world space. It can be moved using W, A, S and D keys and can look
 * around using the mouse.
 */
class FlyCamera
{
public:
    explicit FlyCamera(RndrContext* RndrContext,
                       ProjectionCamera* ProjectionCamera,
                       math::Point3 StartingPosition = math::Point3(),
                       real MovementSpeed = 1.0,
                       real RotationSpeed = 1.0);

    void Update(real DeltaSeconds);

    ProjectionCamera* GetProjectionCamera();
    void SetProjectionCamera(ProjectionCamera* ProjectionCamera);

    [[nodiscard]] math::Point3 GetPosition() const;
    void SetPosition(const math::Point3& Position);

private:
    // Private methods

    void HandleLookVert(InputPrimitive Primitive, InputTrigger Trigger, real AxisValue);
    void HandleLookHorz(InputPrimitive Primitive, InputTrigger Trigger, real AxisValue);
    void HandleMoveForward(InputPrimitive Primitive, InputTrigger Trigger, real Value);
    void HandleMoveRight(InputPrimitive Primitive, InputTrigger Trigger, real Value);

    // Private fields

    ProjectionCamera* m_ProjectionCamera = nullptr;

    math::Point3 m_Position;
    math::Rotator m_DirectionAngles;

    math::Rotator m_DeltaAngles;
    math::Vector3 m_DeltaPosition;

    math::Vector3 m_DirectionVector;
    math::Vector3 m_RightVector;

    real m_MovementSpeed;
    real m_RotationSpeed;
};

}  // namespace rndr