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
    T X, Y, Z;

public:
    Vector3() { X = Y = Z = 0; }
    Vector3(T X, T Y, T Z) : X(X), Y(Y), Z(Z) { assert(!HasNaNs()); }
    explicit Vector3(const Normal3<T>& n);
    explicit Vector3(const Point3<T>& p);

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

    bool operator==(const Vector3<T>& other) const
    {
        return X == other.X && Y == other.Y && Z == other.Z;
    }

    bool operator!=(const Vector3<T>& other) const { return !(*this == other); }

    Vector3<T> operator+(const Vector3<T>& other) const
    {
        return Vector3(X + other.X, Y + other.Y, Z + other.Z);
    }

    Vector3<T>& operator+=(const Vector3<T>& other)
    {
        X += other.X;
        Y += other.Y;
        Z += other.Z;
        return *this;
    }

    Vector3<T> operator-(const Vector3<T>& other) const
    {
        return Vector3(X - other.X, Y - other.Y, Z - other.Z);
    }

    Vector3<T>& operator-=(const Vector3<T>& other)
    {
        X -= other.X;
        Y -= other.Y;
        Z -= other.Z;
        return *this;
    }

    Vector3<T> operator*(T scalar) const { return Vector3(X * scalar, Y * scalar, Z * scalar); }

    Vector3<T>& operator*=(T scalar)
    {
        X *= scalar;
        Y *= scalar;
        Z *= scalar;
        return *this;
    }

    template <typename S>
    Vector3<T> operator/(S scalar) const
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        return Vector3(X * rec, Y * rec, Z * rec);
    }

    template <typename S>
    Vector3<T>& operator/=(S scalar)
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        X *= rec;
        Y *= rec;
        Z *= rec;
        return *this;
    }

    Vector3<T> operator-() const { return Vector3(-X, -Y, -Z); }

    Vector3<T> Abs() const { return Vector3(std::abs(X), std::abs(Y), std::abs(Z)); }

    T LengthSquared() const { return X * X + Y * Y + Z * Z; }
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
    return v1.X * v2.X + v1.Y * v2.Y + v1.Z * v2.Z;
}

template <typename T>
inline T AbsDot(const Vector3<T>& v1, const Vector3<T>& v2)
{
    return std::abs(Dot(v1, v2));
}

template <typename T>
inline Vector3<T> Cross(const Vector3<T>& v1, const Vector3<T>& v2)
{
    double v1X = v1.X, v1Y = v1.Y, v1Z = v1.Z;
    double v2X = v2.X, v2Y = v2.Y, v2Z = v2.Z;
    return Vector3<T>((v1Y * v2Z) - (v1Z * v2Y), (v1Z * v2X) - (v1X * v2Z),
                      (v1X * v2Y) - (v1Y * v2X));
}

template <typename T>
inline real Cross2D(const Vector3<T>& v1, const Vector3<T>& v2)
{
    return v1.X * v2.Y - v2.X * v1.Y;
}

template <typename T>
inline Vector3<T> Normalize(const Vector3<T>& v)
{
    return v / v.Length();
}

template <typename T>
inline T MinComponent(const Vector3<T>& v)
{
    return std::min(v.X, std::min(v.Y, v.Z));
}

template <typename T>
inline T MaxComponent(const Vector3<T>& v)
{
    return std::maX(v.X, std::maX(v.Y, v.Z));
}

template <typename T>
inline int MaxDimension(const Vector3<T>& v)
{
    return (v.X > v.Y) ? (v.X > v.Z ? 0 : 2) : (v.Y > v.Z ? 1 : 2);
}

template <typename T>
Vector3<T> Min(const Vector3<T>& p1, const Vector3<T>& p2)
{
    return Vector3<T>(std::min(p1.X, p2.X), std::min(p1.Y, p2.Y), std::min(p1.Z, p2.Z));
}

template <typename T>
Vector3<T> Max(const Vector3<T>& v1, const Vector3<T>& v2)
{
    return Vector3<T>(std::max(v1.X, v2.X), std::max(v1.Y, v2.Y), std::max(v1.Z, v2.Z));
}

template <typename T>
Vector3<T> Permute(const Vector3<T>& v, int X, int Y, int Z)
{
    return Vector3<T>(v[X], v[Y], v[Z]);
}

template <typename T>
inline void CoordinateSystem(const Vector3<T>& v1, Vector3<T>* v2, Vector3<T>* v3)
{
    if (std::abs(v1.X) > std::abs(v1.Y))
    {
        *v2 = Vector3<T>(-v1.Z, 0, v1.X) / std::sqrt(v1.X * v1.X + v1.Z * v1.Z);
    }
    else
    {
        *v2 = Vector3<T>(0, v1.Z, -v1.Y) / std::sqrt(v1.Y * v1.Y + v1.Z * v1.Z);
    }
    *v3 = Cross(v1, *v2);
}

}  // namespace rndr