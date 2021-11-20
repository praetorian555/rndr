// MIT License
//
// Copyright (c) 2021 Marko Kostić
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <numeric>

#include "rndr/rndr.h"

#include "rndr/math/matrix4x4.h"
#include "rndr/math/normal3.h"
#include "rndr/math/point2.h"
#include "rndr/math/point3.h"
#include "rndr/math/rng.h"
#include "rndr/math/vector2.h"
#include "rndr/math/vector3.h"

namespace rndr
{

// Types

using Vector2i = Vector2<int>;
using Vector2r = Vector2<real>;
using Vector3i = Vector3<int>;
using Vector3r = Vector3<real>;

using Point2i = Point2<int>;
using Point2r = Point2<real>;
using Point3i = Point3<int>;
using Point3r = Point3<real>;

using Normal3r = Normal3<real>;

// Constants

static constexpr real Pi = 3.14159265358979323846;
static constexpr real InvPi = 0.31830988618379067154;
static constexpr real Inv2Pi = 0.15915494309189533577;
static constexpr real Inv4Pi = 0.07957747154594766788;
static constexpr real PiOver2 = 1.57079632679489661923;
static constexpr real PiOver4 = 0.78539816339744830961;
static constexpr real Sqrt2 = 1.41421356237309504880;
static constexpr real Infinity = std::numeric_limits<real>::infinity();

// Functions

template <typename T>
inline bool IsNaN(const T v)
{
    return std::isnan(v);
}

template <>
inline bool IsNaN<int>(const int v)
{
    return false;
}

template <typename T, typename V, typename U>
T Clamp(T val, V low, U high);

template <typename T>
T Mod(T a, T b);

template <typename T>
T Lerp(real t, T p0, T p1);

template <typename T>
bool IsPowerOf2(T v);

real Radians(real deg);
real Degrees(real rad);

real Log2(real x);
int32_t Log2Int(uint32_t v);

int32_t RoundUpPow2(int32_t v);

int32_t CountTrailingZeros(uint32_t v);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Implementations

template <typename T>
Vector3<T>::Vector3(const Normal3<T>& n) : x(n.x), y(n.y), z(n.z)
{
}

template <typename T>
Vector3<T>::Vector3(const Point3<T>& p) : x(p.x), y(p.y), z(p.z)
{
}

template <typename T, typename V, typename U>
T Clamp(T val, V low, U high)
{
    return val < low ? low : (val > high ? high : val);
}

template <typename T>
T Mod(T a, T b)
{
    T result = a - (a / b) * b;
    return (T)((result < 0) ? result + b : result);
}

template <typename T>
T Lerp(real t, T p0, T p1)
{
    return (1 - t) * p0 + t * p1;
}

template <typename T>
inline bool IsPowerOf2(T v)
{
    return v && !(v & (v - 1));
}

}  // namespace pbr
