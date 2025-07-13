#pragma once

#include "opal/math-base.h"
#include "opal/math/bounds2.h"
#include "opal/math/bounds3.h"
#include "opal/math/matrix.h"
#include "opal/math/normal3.h"
#include "opal/math/quaternion.h"
#include "opal/math/rotator.h"
#include "opal/math/vector3.h"

#include "rndr/types.hpp"

// Math types
namespace Rndr
{
using Point2f = Opal::Point2<f32>;
using Point3f = Opal::Point3<f32>;
using Point4f = Opal::Point4<f32>;
using Vector2f = Opal::Vector2<f32>;
using Vector3f = Opal::Vector3<f32>;
using Vector4f = Opal::Vector4<f32>;
using Normal3f = Opal::Normal3<f32>;
using Matrix4x4f = Opal::Matrix4x4<f32>;
using Rotatorf = Opal::Rotator<f32>;
using Quaternionf = Opal::Quaternion<f32>;
using Bounds2f = Opal::Bounds2<f32>;
using Bounds3f = Opal::Bounds3<f32>;

using Point2i = Opal::Point2<int32_t>;
using Vector2i = Opal::Vector2<int32_t>;
}  // namespace Rndr
