#pragma once

#include "math/math.h"

#include "rndr/core/base.h"

// Math types
namespace Rndr
{

using Point2f = Math::Point2<float>;
using Point3f = Math::Point3<float>;
using Point4f = Math::Point4<float>;
using Vector2f = Math::Vector2<float>;
using Vector3f = Math::Vector3<float>;
using Vector4f = Math::Vector4<float>;
using Normal3f = Math::Normal3<float>;
using Matrix4x4f = Math::Matrix4x4<float>;
using Rotatorf = Math::Rotator<float>;
using Quaternionf = Math::Quaternion<float>;

} // namespace Rndr
