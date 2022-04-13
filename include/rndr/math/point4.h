#pragma once

#include <cmath>

#include "rndr/math/Vector4.h"

namespace rndr
{

template <typename T>
class Point3;

template <typename T>
inline bool IsNaN(const T Vec);

template <typename T>
class Point4
{
public:
    T X, Y, Z, W;

public:
    Point4() { X = Y = Z = W = 0; }
    Point4(T X, T Y, T Z, T W) : X(X), Y(Y), Z(Z), W(W) { assert(!HasNaNs()); }

    template <typename U>
    explicit Point4(const Point4<U>& p) : X((T)p.X), Y((T)p.Y), Z((T)p.Z), W((T)p.W)
    {
        Assert(!HasNaNs());
    }

    template <typename U>
    explicit Point4(const Point3<U>& p);

    void Set(T Val, int i)
    {
        assert(i >= 0 && i < 4);
        if (i == 0)
            X = Val;
        if (i == 1)
            Y = Val;
        if (i == 2)
            Z = Val;
        if (i == 3)
            W = Val;
    }

    T operator[](int i) const
    {
        assert(i >= 0 && i < 4);
        if (i == 0)
            return X;
        if (i == 1)
            return Y;
        if (i == 2)
            return Z;
        if (i == 3)
            return W;
    }

    bool HasNaNs() const { return IsNaN(X) || IsNaN(Y) || IsNaN(Z) || IsNaN(W); }

    bool operator==(const Point4<T>& Other) const { return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W; }

    bool operator!=(const Point4<T>& Other) const { return !(*this == Other); }

    Point4<T> operator+(const Vector4<T>& Vec) const { return Point4(X + Vec.X, Y + Vec.Y, Z + Vec.Z, W + Vec.W); }

    Point4<T>& operator+=(const Vector4<T>& Vec)
    {
        X += Vec.X;
        Y += Vec.Y;
        Z += Vec.Z;
        W += Vec.W;
        return *this;
    }

    Point4<T> operator+(const Point4<T>& Other) const { return Point4(X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W); }

    Point4<T>& operator+=(const Point4<T>& Other)
    {
        X += Other.X;
        Y += Other.Y;
        Z += Other.Z;
        W += Other.W;
        return *this;
    }

    template <typename U>
    Point4<T> operator+(const U& Value) const
    {
        return Point4(X + Value, Y + Value, Z + Value, W + Value);
    }

    template <typename U>
    Point4<T>& operator+=(const U& Value)
    {
        X += Value;
        Y += Value;
        Z += Value;
        W += Value;
        return *this;
    }

    Point4<T> operator-(const Vector4<T>& Vec) const { return Point4(X - Vec.X, Y - Vec.Y, Z - Vec.Z, W - Vec.W); }

    Vector4<T> operator-(const Point4<T>& Other) const { return Vector4(X - Other.X, Y - Other.Y, Z - Other.Z, W - Other.W); }

    Point4<T>& operator-=(const Vector4<T>& Vec)
    {
        X -= Vec.X;
        Y -= Vec.Y;
        Z -= Vec.Z;
        W -= Vec.W;
        return *this;
    }

    Point4<T> operator*(T scalar) const { return Point4(X * scalar, Y * scalar, Z * scalar, W * scalar); }

    Point4<T>& operator*=(T scalar)
    {
        X *= scalar;
        Y *= scalar;
        Z *= scalar;
        W *= scalar;
        return *this;
    }

    template <typename S>
    Point4<T> operator/(S scalar) const
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        return Point4(X * rec, Y * rec, Z * rec, W * rec);
    }

    template <typename S>
    Point4<T>& operator/=(S scalar)
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        X *= rec;
        Y *= rec;
        Z *= rec;
        W *= rec;
        return *this;
    }

    Point4<T> operator-() const { return Point4(-X, -Y, -Z, -W); }

    Point4<T> Abs() const { return Point4(std::abs(X), std::abs(Y), std::abs(Z), std::abs(W)); }

    Point4<T> ToEuclidean() const
    {
        assert(W != 0);
        real Div = 1 / W;
        return Point4(X * Div, Y * Div, Z * Div, 1);
    }
};

template <typename T>
inline Point4<T> operator*(T scalar, const Point4<T>& Vec)
{
    return Vec * scalar;
}

template <typename T>
inline float Distance(const Point4<T>& p1, const Point4<T>& p2)
{
    return (p1 - p2).Length();
}
template <typename T>
inline float DistanceSquared(const Point4<T>& p1, const Point4<T>& p2)
{
    return (p1 - p2).LengthSquared();
}

template <typename T>
Point4<T> Lerp(float t, const Point4<T>& p0, const Point4<T>& p1)
{
    return (1 - t) * p0 + t * p1;
}

template <typename T>
Point4<T> Floor(const Point4<T>& p)
{
    return Point4<T>(std::floor(p.X), std::floor(p.Y), std::floor(p.Z), std::floor(p.W));
}
template <typename T>
Point4<T> Ceil(const Point4<T>& p)
{
    return Point4<T>(std::ceil(p.X), std::ceil(p.Y), std::ceil(p.Z), std::ceil(p.W));
}

template <typename T>
Point4<T> Min(const Point4<T>& p1, const Point4<T>& p2)
{
    return Point4<T>(std::min(p1.X, p2.X), std::min(p1.Y, p2.Y), std::min(p1.Z, p2.Z), std::min(p1.W, p2.W));
}

template <typename T>
Point4<T> Max(const Point4<T>& Vec1, const Point4<T>& Vec2)
{
    return Point4<T>(std::max(Vec1.X, Vec2.X), std::max(Vec1.Y, Vec2.Y), std::max(Vec1.Z, Vec2.Z), std::max(Vec1.W, Vec2.W));
}

template <typename T>
Point4<T> Permute(const Point4<T>& Vec, int X, int Y, int Z, int W)
{
    return Point4<T>(Vec[X], Vec[Y], Vec[Z], Vec[W]);
}

}  // namespace rndr
