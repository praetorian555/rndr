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

#include "rndr/math/point3.h"
#include "rndr/math/vector2.h"

namespace rndr
{

template <typename T>
inline bool IsNaN(const T v);

template <typename T>
class Point2
{
public:
    T x, y;

public:
    Point2() { x = y = 0; }
    Point2(T x, T y) : x(x), y(y) { assert(!HasNaNs()); }
    Point2(const Point2<T>& p3) : x(p3.x), y(p3.y) { assert(!HasNaNs()); }

    template <typename U>
    explicit Point2(const Point2<U>& p) : x((T)p.x), y((T)p.y)
    {
        assert(!HasNaNs());
    }

    void Set(T val, int i)
    {
        assert(i >= 0 && i < 2);
        if (i == 0)
            x = val;
        if (i == 1)
            y = val;
    }

    T operator[](int i) const
    {
        assert(i >= 0 && i < 2);
        if (i == 0)
            return x;
        if (i == 1)
            return y;
    }

    bool HasNaNs() const { return IsNaN(x) || IsNaN(y); }

    bool operator==(const Point2<T>& other) const { return x == other.x && y == other.y; }

    bool operator!=(const Point2<T>& other) const { return !(*this == other); }

    Point2<T> operator+(const Vector2<T>& v) const { return Point2(x + v.x, y + v.y); }

    Point2<T>& operator+=(const Vector2<T>& v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }

    Point2<T> operator+(const Point2<T>& other) const { return Point2(x + other.x, y + other.y); }

    Point2<T>& operator+=(const Point2<T>& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    Point2<T> operator-(const Vector2<T>& v) const { return Point2(x - v.x, y - v.y); }

    Vector2<T> operator-(const Point2<T>& other) const { return Vector2(x - other.x, y - other.y); }

    Point2<T>& operator-=(const Vector2<T>& v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }

    Point2<T> operator*(T scalar) const { return Point2(x * scalar, y * scalar); }

    Point2<T>& operator*=(T scalar)
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    template <typename S>
    Point2<T> operator/(S scalar) const
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        return Point2(x * rec, y * rec);
    }

    template <typename S>
    Point2<T>& operator/=(S scalar)
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        x *= rec;
        y *= rec;
        return *this;
    }

    Point2<T> operator-() const { return Point2(-x, -y); }

    Point2<T> Abs() const { return Point2(std::abs(x), std::abs(y)); }
};

template <typename T>
inline Point2<T> operator*(T scalar, const Point2<T>& v)
{
    return v * scalar;
}

template <typename T>
inline float Distance(const Point2<T>& p1, const Point2<T>& p2)
{
    return (p1 - p2).Length();
}
template <typename T>
inline float DistanceSquared(const Point2<T>& p1, const Point2<T>& p2)
{
    return (p1 - p2).LengthSquared();
}

template <typename T>
Point2<T> Lerp(float t, const Point2<T>& p0, const Point2<T>& p1)
{
    return (1 - t) * p0 + t * p1;
}

template <typename T>
Point2<T> Floor(const Point2<T>& p)
{
    return Point2<T>(std::floor(p.x), std::floor(p.y));
}
template <typename T>
Point2<T> Ceil(const Point2<T>& p)
{
    return Point2<T>(std::ceil(p.x), std::ceil(p.y));
}

template <typename T>
Point2<T> Min(const Point2<T>& v1, const Point2<T>& v2)
{
    return Point2<T>(std::min(v1.x, v2.x), std::min(v1.y, v2.y));
}

template <typename T>
Point2<T> Max(const Point2<T>& v1, const Point2<T>& v2)
{
    return Point2<T>(std::max(v1.x, v2.x), std::max(v1.y, v2.y));
}

template <typename T>
Point2<T> Permute(const Point2<T>& v, int x, int y)
{
    return Point2<T>(v[x], v[y]);
}

}  // namespace pbr