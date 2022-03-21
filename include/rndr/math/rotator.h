#pragma once

#include "rndr/core/base.h"

#include "rndr/math/vector3.h"

namespace rndr
{

/**
 * Orientation expressed in angle rotations around axes of coordinate system. Always in degrees.
 * Default orientation (all zeros) means that the forward vector is along the negative z-axis, right
 * vector is along x-axis and up vector is along the y-axis.
 */
struct Rotator
{
    /**
     * Rotation around the right axis (around Z axis), Looking up and down (0=Straight Ahead, +Up,
     * -Down).
     */
    real Pitch = 0;

    /** Rotation around the up axis (around Y axis), Running in circles +Left, -Right. */
    real Yaw = 0;

    /**
     * Rotation around the forward axis (around X axis), Tilting your head, 0=Straight, +CW, -CCW.
     */
    real Roll = 0;

    Rotator() = default;
    Rotator(real Pitch, real Yaw, real Roll);

    /**
     * Returns normalized vector in the direction defined by the rotator.
     */
     Vector3<real> ToVector();

     Rotator operator+(Rotator Other) const;
     Rotator& operator+=(Rotator Other);
};

}  // namespace rndr