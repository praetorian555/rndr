#include "rndr/math/rotator.h"

#include "rndr/core/math.h"

rndr::Rotator::Rotator(real Pitch, real Yaw, real Roll) : Pitch(Pitch), Yaw(Yaw), Roll(Roll) {}

rndr::Vector3<real> rndr::Rotator::ToVector()
{
    const real PitchNoWinding = std::fmod(Pitch, 360.0f);
    const real YawNoWinding = std::fmod(Yaw, 360.0f);

    const real SP = std::sin(rndr::Radians(PitchNoWinding));
    const real CP = std::cos(rndr::Radians(PitchNoWinding));
    const real SY = std::sin(rndr::Radians(YawNoWinding));
    const real CY = std::cos(rndr::Radians(YawNoWinding));

    return Vector3<real>{CP * CY, SP, -CP * SY};
}

rndr::Rotator rndr::Rotator::operator+(Rotator Other) const
{
    return Rotator{Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll};
}

rndr::Rotator& rndr::Rotator::operator+=(Rotator Other)
{
    Pitch += Other.Pitch;
    Yaw += Other.Yaw;
    Roll += Other.Roll;

    return *this;
}
