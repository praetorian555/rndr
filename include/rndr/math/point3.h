#pragma once

#include <cmath>

#include "rndr/math/vector3.h"

namespace rndr
{

template <typename T>
class Point2;

template <typename T>
inline bool IsNaN(const T v);

template <typename T>
class Point3
{
public:
    T X, Y, Z;

public:
    Point3() { X = Y = Z = 0; }
    Point3(T X, T Y, T Z) : X(X), Y(Y), Z(Z) { assert(!HasNaNs()); }

    template <typename U>
    explicit Point3(const Point3<U>& p) : X((T)p.X), Y((T)p.Y), Z((T)p.Z)
    {
        Assert(!HasNaNs());
    }

    template <typename U>
    explicit Point3(const Point2<U>& p);

    void Set(T val, int i)
    {
        assert(i >= 0 && i < 3);
        if (i == 0)
            X = val;
        if (i == 1)
            Y = val;
        if (i == 2)
            Z = val;
    }

    T operator[](int i) const
    {
        assert(i >= 0 && i < 3);
        if (i == 0)
            return X;
        if (i == 1)
            return Y;
        if (i == 2)
            return Z;
    }

    bool HasNaNs() const { return IsNaN(X) || IsNaN(Y) || IsNaN(Z); }

    bool operator==(const Point3<T>& other) const
    {
        return X == other.X && Y == other.Y && Z == other.Z;
    }

    bool operator!=(const Point3<T>& other) const { return !(*this == other); }

    Point3<T> operator+(const Vector3<T>& v) const { return Point3(X + v.X, Y + v.Y, Z + v.Z); }

    Point3<T>& operator+=(const Vector3<T>& v)
    {
        X += v.X;
        Y += v.Y;
        Z += v.Z;
        return *this;
    }

    Point3<T> operator+(const Point3<T>& other) const
    {
        return Point3(X + other.X, Y + other.Y, Z + other.Z);
    }

    Point3<T>& operator+=(const Point3<T>& other)
    {
        X += other.X;
        Y += other.Y;
        Z += other.Z;
        return *this;
    }

    template <typename U>
    Point3<T> operator+(const U& Value) const
    {
        return Point3(X + Value, Y + Value, Z + Value);
    }

    template <typename U>
    Point3<T>& operator+=(const U& Value)
    {
        X += Value;
        Y += Value;
        Z += Value;
        return *this;
    }

    Point3<T> operator-(const Vector3<T>& v) const { return Point3(X - v.X, Y - v.Y, Z - v.Z); }

    Vector3<T> operator-(const Point3<T>& other) const
    {
        return Vector3(X - other.X, Y - other.Y, Z - other.Z);
    }

    Point3<T>& operator-=(const Vector3<T>& v)
    {
        X -= v.X;
        Y -= v.Y;
        Z -= v.Z;
        return *this;
    }

    Point3<T> operator*(T scalar) const { return Point3(X * scalar, Y * scalar, Z * scalar); }

    Point3<T>& operator*=(T scalar)
    {
        X *= scalar;
        Y *= scalar;
        Z *= scalar;
        return *this;
    }

    template <typename S>
    Point3<T> operator/(S scalar) const
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        return Point3(X * rec, Y * rec, Z * rec);
    }

    template <typename S>
    Point3<T>& operator/=(S scalar)
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        X *= rec;
        Y *= rec;
        Z *= rec;
        return *this;
    }

    Point3<T> operator-() const { return Point3(-X, -Y, -Z); }

    Point3<T> Abs() const { return Point3(std::abs(X), std::abs(Y), std::abs(Z)); }
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
    return Point3<T>(std::floor(p.X), std::floor(p.Y), std::floor(p.Z));
}
template <typename T>
Point3<T> Ceil(const Point3<T>& p)
{
    return Point3<T>(std::ceil(p.X), std::ceil(p.Y), std::ceil(p.Z));
}

template <typename T>
Point3<T> Min(const Point3<T>& p1, const Point3<T>& p2)
{
    return Point3<T>(std::min(p1.X, p2.X), std::min(p1.Y, p2.Y), std::min(p1.Z, p2.Z));
}

template <typename T>
Point3<T> Max(const Point3<T>& v1, const Point3<T>& v2)
{
    return Point3<T>(std::max(v1.X, v2.X), std::max(v1.Y, v2.Y), std::max(v1.Z, v2.Z));
}

template <typename T>
Point3<T> Permute(const Point3<T>& v, int X, int Y, int Z)
{
    return Point3<T>(v[X], v[Y], v[Z]);
}

}  // namespace rndr
