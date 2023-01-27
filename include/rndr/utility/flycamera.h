#pragma once

#include "math/point3.h"
#include "math/rotator.h"
#include "math/vector3.h"

#include "rndr/core/base.h"
#include "rndr/core/projectioncamera.h"
#include "rndr/core/input.h"

namespace rndr
{

struct FlyCameraProperties
{
    math::Vector3 StartPosition;
    math::Rotator StartRotation;
    real MovementSpeed = RNDR_REALC(1.0);
    real RotationSpeed = RNDR_REALC(2.0);
    ProjectionCameraProperties ProjectionProps;
};

/**
 * Represents camera in the world space. It can be moved using W, A, S and D keys and can look
 * around using the mouse.
 */
class FlyCamera : public ProjectionCamera
{
public:
    explicit FlyCamera(InputContext* InputContext,
                       int ScreenWidth,
                       int ScreenHeight,
                       const FlyCameraProperties& Props = FlyCameraProperties{});

    void Update(real DeltaSeconds);

    [[nodiscard]] math::Point3 GetPosition() const;

private:
    // Private methods

    void HandleLookVert(InputPrimitive Primitive, InputTrigger Trigger, real AxisValue);
    void HandleLookHorz(InputPrimitive Primitive, InputTrigger Trigger, real AxisValue);
    void HandleMoveForward(InputPrimitive Primitive, InputTrigger Trigger, real Value);
    void HandleMoveRight(InputPrimitive Primitive, InputTrigger Trigger, real Value);

    // Private fields

    InputContext* m_InputContext;
    FlyCameraProperties m_Props;

    math::Point3 m_Position;
    math::Rotator m_DirectionAngles;

    math::Rotator m_DeltaAngles;
    math::Vector3 m_DeltaPosition;

    math::Vector3 m_DirectionVector;
    math::Vector3 m_RightVector;
};

}  // namespace rndr