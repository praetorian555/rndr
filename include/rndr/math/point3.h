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

#include <cmath>

#include "rndr/math/vector3.h"

namespace rndr
{

template <typename T>
inline bool IsNaN(const T v);

template <typename T>
class Point3
{
public:
    T x, y, z;

public:
    Point3() { x = y = z = 0; }
    Point3(T x, T y, T z) : x(x), y(y), z(z) { assert(!HasNaNs()); }

    template <typename U>
    explicit Point3(const Point3<U>& p) : x((T)p.x), y((T)p.y), z((T)p.z)
    {
        Assert(!HasNaNs());
    }

    void Set(T val, int i)
    {
        assert(i >= 0 && i < 3);
        if (i == 0)
            x = val;
        if (i == 1)
            y = val;
        if (i == 2)
            z = val;
    }

    T operator[](int i) const
    {
        assert(i >= 0 && i < 3);
        if (i == 0)
            return x;
        if (i == 1)
            return y;
        if (i == 2)
            return z;
    }

    bool HasNaNs() const { return IsNaN(x) || IsNaN(y) || IsNaN(z); }

    bool operator==(const Point3<T>& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Point3<T>& other) const { return !(*this == other); }

    Point3<T> operator+(const Vector3<T>& v) const { return Point3(x + v.x, y + v.y, z + v.z); }

    Point3<T>& operator+=(const Vector3<T>& v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    Point3<T> operator+(const Point3<T>& other) const
    {
        return Point3(x + other.x, y + other.y, z + other.z);
    }

    Point3<T>& operator+=(const Point3<T>& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Point3<T> operator-(const Vector3<T>& v) const { return Point3(x - v.x, y - v.y, z - v.z); }

    Vector3<T> operator-(const Point3<T>& other) const
    {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Point3<T>& operator-=(const Vector3<T>& v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    Point3<T> operator*(T scalar) const { return Point3(x * scalar, y * scalar, z * scalar); }

    Point3<T>& operator*=(T scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    template <typename S>
    Point3<T> operator/(S scalar) const
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        return Point3(x * rec, y * rec, z * rec);
    }

    template <typename S>
    Point3<T>& operator/=(S scalar)
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        x *= rec;
        y *= rec;
        z *= rec;
        return *this;
    }

    Point3<T> operator-() const { return Point3(-x, -y, -z); }

    Point3<T> Abs() const { return Point3(std::abs(x), std::abs(y), std::abs(z)); }
};

template <typename T>
inline Point3<T> operator*(T scalar, const Point3<T>& v)
{
    return v * scalar;
}

template <typename T>
inline float Distance(const Point3<T>& p1, const Point3<T>& p2)
{
    return (p1 - p2).Length();
}
template <typename T>
inline float DistanceSquared(const Point3<T>& p1, const Point3<T>& p2)
{
    return (p1 - p2).LengthSquared();
}

template <typename T>
Point3<T> Lerp(float t, const Point3<T>& p0, const Point3<T>& p1)
{
    return (1 - t) * p0 + t * p1;
}

template <typename T>
Point3<T> Floor(const Point3<T>& p)
{
    return Point3<T>(std::floor(p.x), std::floor(p.y), std::floor(p.z));
}
template <typename T>
Point3<T> Ceil(const Point3<T>& p)
{
    return Point3<T>(std::ceil(p.x), std::ceil(p.y), std::ceil(p.z));
}

template <typename T>
Point3<T> Min(const Point3<T>& p1, const Point3<T>& p2)
{
    return Point3<T>(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::min(p1.z, p2.z));
}

template <typename T>
Point3<T> Max(const Point3<T>& v1, const Point3<T>& v2)
{
    return Point3<T>(std::max(v1.x, v2.x), std::max(v1.y, v2.y), std::max(v1.z, v2.z));
}

template <typename T>
Point3<T> Permute(const Point3<T>& v, int x, int y, int z)
{
    return Point3<T>(v[x], v[y], v[z]);
}

}  // namespace rndr
