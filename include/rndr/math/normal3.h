#pragma once

#include <cmath>

#include "rndr/math/vector3.h"

namespace rndr
{

template <typename T>
inline bool IsNaN(const T v);

template <typename T>
class Normal3
{
public:
    T X, Y, Z;

public:
    Normal3() { X = Y = Z = 0; }
    Normal3(T X, T Y, T Z) : X(X), Y(Y), Z(Z) { assert(!HasNaNs()); }
    explicit Normal3(const Vector3<T>& v) : X(v.X), Y(v.Y), Z(v.Z) { assert(!HasNaNs()); }

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

    bool operator==(const Normal3<T>& other) const { return X == other.X && Y == other.Y && Z == other.Z; }

    bool operator!=(const Normal3<T>& other) const { return !(*this == other); }

    Normal3<T> operator+(const Normal3<T>& other) const { return Normal3(X + other.X, Y + other.Y, Z + other.Z); }

    Normal3<T>& operator+=(const Normal3<T>& other)
    {
        X += other.X;
        Y += other.Y;
        Z += other.Z;
        return *this;
    }

    Normal3<T> operator-(const Normal3<T>& other) const { return Normal3(X - other.X, Y - other.Y, Z - other.Z); }

    Normal3<T>& operator-=(const Normal3<T>& other)
    {
        X -= other.X;
        Y -= other.Y;
        Z -= other.Z;
        return *this;
    }

    Normal3<T> operator*(T scalar) const { return Normal3(X * scalar, Y * scalar, Z * scalar); }

    Normal3<T>& operator*=(T scalar)
    {
        X *= scalar;
        Y *= scalar;
        Z *= scalar;
        return *this;
    }

    template <typename S>
    Normal3<T> operator/(S scalar) const
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        return Normal3(X * rec, Y * rec, Z * rec);
    }

    template <typename S>
    Normal3<T>& operator/=(S scalar)
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        X *= rec;
        Y *= rec;
        Z *= rec;
        return *this;
    }

    Normal3<T> operator-() const { return Normal3(-X, -Y, -Z); }

    Normal3<T> Abs() const { return Normal3(std::abs(X), std::abs(Y), std::abs(Z)); }

    T LengthSquared() const { return X * X + Y * Y + Z * Z; }
    T Length() const { return std::sqrt(LengthSquared()); }
};

template <typename T>
inline Normal3<T> operator*(T scalar, const Normal3<T>& v)
{
    return v * scalar;
}

template <typename T>
inline T Dot(const Normal3<T>& v1, const Normal3<T>& v2)
{
    return v1.X * v2.X + v1.Y * v2.Y + v1.Z * v2.Z;
}

template <typename T>
inline T Dot(const Normal3<T>& v1, const Vector3<T>& v2)
{
    return v1.X * v2.X + v1.Y * v2.Y + v1.Z * v2.Z;
}

template <typename T>
inline T Dot(const Vector3<T>& v1, const Normal3<T>& v2)
{
    return v1.X * v2.X + v1.Y * v2.Y + v1.Z * v2.Z;
}

template <typename T>
inline T AbsDot(const Normal3<T>& v1, const Normal3<T>& v2)
{
    return std::abs(Dot(v1, v2));
}

template <typename T>
inline T AbsDot(const Normal3<T>& v1, const Vector3<T>& v2)
{
    return std::abs(Dot(v1, v2));
}

template <typename T>
inline T AbsDot(const Vector3<T>& v1, const Normal3<T>& v2)
{
    return std::abs(Dot(v1, v2));
}

template <typename T>
inline Normal3<T> Normalize(const Normal3<T>& v)
{
    return v / v.Length();
}

template <typename T>
inline T MinComponent(const Normal3<T>& v)
{
    return std::min(v.X, std::min(v.Y, v.Z));
}

template <typename T>
inline T MaxComponent(const Normal3<T>& v)
{
    return std::max(v.X, std::max(v.Y, v.Z));
}

template <typename T>
inline int MaxDimension(const Normal3<T>& v)
{
    return (v.X > v.Y) ? (v.X > v.Z ? 0 : 2) : (v.Y > v.Z ? 1 : 2);
}

template <typename T>
Normal3<T> Min(const Normal3<T>& p1, const Normal3<T>& p2)
{
    return Normal3<T>(std::min(p1.X, p2.X), std::min(p1.Y, p2.Y), std::min(p1.Z, p2.Z));
}

template <typename T>
Normal3<T> Max(const Normal3<T>& v1, const Normal3<T>& v2)
{
    return Normal3<T>(std::max(v1.X, v2.X), std::max(v1.Y, v2.Y), std::max(v1.Z, v2.Z));
}

template <typename T>
Normal3<T> Permute(const Normal3<T>& v, int X, int Y, int Z)
{
    return Normal3<T>(v[X], v[Y], v[Z]);
}

template <typename T>
inline Normal3<T> Faceforward(const Normal3<T>& n, const Vector3<T>& v)
{
    return (Dot(n, v) < 0.f) ? -n : n;
}

}  // namespace rndr