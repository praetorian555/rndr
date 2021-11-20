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

#include <cassert>
#include <cmath>

namespace rndr
{

template <typename T>
inline bool IsNaN(const T v);

template <typename T>
class Normal3;
template <typename T>
class Point3;

template <typename T>
class Vector3
{
public:
    T x, y, z;

public:
    Vector3() { x = y = z = 0; }
    Vector3(T x, T y, T z) : x(x), y(y), z(z) { assert(!HasNaNs()); }
    explicit Vector3(const Normal3<T>& n);
    explicit Vector3(const Point3<T>& p);

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

    bool operator==(const Vector3<T>& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Vector3<T>& other) const { return !(*this == other); }

    Vector3<T> operator+(const Vector3<T>& other) const
    {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3<T>& operator+=(const Vector3<T>& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vector3<T> operator-(const Vector3<T>& other) const
    {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Vector3<T>& operator-=(const Vector3<T>& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    Vector3<T> operator*(T scalar) const { return Vector3(x * scalar, y * scalar, z * scalar); }

    Vector3<T>& operator*=(T scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    template <typename S>
    Vector3<T> operator/(S scalar) const
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        return Vector3(x * rec, y * rec, z * rec);
    }

    template <typename S>
    Vector3<T>& operator/=(S scalar)
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        x *= rec;
        y *= rec;
        z *= rec;
        return *this;
    }

    Vector3<T> operator-() const { return Vector3(-x, -y, -z); }

    Vector3<T> Abs() const { return Vector3(std::abs(x), std::abs(y), std::abs(z)); }

    T LengthSquared() const { return x * x + y * y + z * z; }
    T Length() const { return std::sqrt(LengthSquared()); }
};

template <typename T>
inline Vector3<T> operator*(T scalar, const Vector3<T>& v)
{
    return v * scalar;
}

template <typename T>
inline T Dot(const Vector3<T>& v1, const Vector3<T>& v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

template <typename T>
inline T AbsDot(const Vector3<T>& v1, const Vector3<T>& v2)
{
    return std::abs(Dot(v1, v2));
}

template <typename T>
inline Vector3<T> Cross(const Vector3<T>& v1, const Vector3<T>& v2)
{
    double v1x = v1.x, v1y = v1.y, v1z = v1.z;
    double v2x = v2.x, v2y = v2.y, v2z = v2.z;
    return Vector3<T>((v1y * v2z) - (v1z * v2y), (v1z * v2x) - (v1x * v2z),
                      (v1x * v2y) - (v1y * v2x));
}

template <typename T>
inline Vector3<T> Normalize(const Vector3<T>& v)
{
    return v / v.Length();
}

template <typename T>
inline T MinComponent(const Vector3<T>& v)
{
    return std::min(v.x, std::min(v.y, v.z));
}

template <typename T>
inline T MaxComponent(const Vector3<T>& v)
{
    return std::max(v.x, std::max(v.y, v.z));
}

template <typename T>
inline int MaxDimension(const Vector3<T>& v)
{
    return (v.x > v.y) ? (v.x > v.z ? 0 : 2) : (v.y > v.z ? 1 : 2);
}

template <typename T>
Vector3<T> Min(const Vector3<T>& p1, const Vector3<T>& p2)
{
    return Vector3<T>(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::min(p1.z, p2.z));
}

template <typename T>
Vector3<T> Max(const Vector3<T>& v1, const Vector3<T>& v2)
{
    return Vector3<T>(std::max(v1.x, v2.x), std::max(v1.y, v2.y), std::max(v1.z, v2.z));
}

template <typename T>
Vector3<T> Permute(const Vector3<T>& v, int x, int y, int z)
{
    return Vector3<T>(v[x], v[y], v[z]);
}

template <typename T>
inline void CoordinateSystem(const Vector3<T>& v1, Vector3<T>* v2, Vector3<T>* v3)
{
    if (std::abs(v1.x) > std::abs(v1.y))
    {
        *v2 = Vector3<T>(-v1.z, 0, v1.x) / std::sqrt(v1.x * v1.x + v1.z * v1.z);
    }
    else
    {
        *v2 = Vector3<T>(0, v1.z, -v1.y) / std::sqrt(v1.y * v1.y + v1.z * v1.z);
    }
    *v3 = Cross(v1, *v2);
}

}  // namespace rndr