#pragma once

#include <cassert>
#include <cmath>

namespace rndr
{

template <typename T>
inline bool IsNaN(const T v);

template <typename T>
class Vector2;
template <typename T>
class Vector3;
template <typename T>
class Normal3;
template <typename T>
class Point4;

template <typename T>
class Vector4
{
public:
    union
    {
        struct
        {
            T X, Y, Z, W;
        };
        struct
        {
            T R, G, B, A;
        };
        T Elements[4];
    };

public:
    Vector4() { X = Y = Z = W = 0; }
    Vector4(T X, T Y, T Z, T W) : X(X), Y(Y), Z(Z), W(W) { assert(!HasNaNs()); }
    explicit Vector4(const Point4<T>& p);

    explicit Vector4(const Vector2<T>& XY, T Z = 0, T W = 0);
    explicit Vector4(const Vector3<T>& XYZ, T W = 0);

    Vector2<T> XY() const;
    Vector2<T> XZ() const;
    Vector2<T> XW() const;
    Vector2<T> YZ() const;
    Vector2<T> YW() const;
    Vector2<T> ZW() const;

    Vector3<T> XYZ() const;
    Vector3<T> XYW() const;
    Vector3<T> XZW() const;
    Vector3<T> YZW() const;

    void Set(T val, int i)
    {
        assert(i >= 0 && i < 4);
        if (i == 0)
            X = val;
        if (i == 1)
            Y = val;
        if (i == 2)
            Z = val;
        if (i == 3)
            W = val;
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

    bool operator==(const Vector4<T>& other) const { return X == other.X && Y == other.Y && Z == other.Z && W == other.W; }

    bool operator!=(const Vector4<T>& other) const { return !(*this == other); }

    Vector4<T> operator+(const Vector4<T>& other) const { return Vector4(X + other.X, Y + other.Y, Z + other.Z, W + other.W); }

    Vector4<T>& operator+=(const Vector4<T>& other)
    {
        X += other.X;
        Y += other.Y;
        Z += other.Z;
        W += other.W;
        return *this;
    }

    Vector4<T> operator-(const Vector4<T>& other) const { return Vector4(X - other.X, Y - other.Y, Z - other.Z, W + other.W); }

    Vector4<T>& operator-=(const Vector4<T>& other)
    {
        X -= other.X;
        Y -= other.Y;
        Z -= other.Z;
        W -= other.W;
        return *this;
    }

    Vector4<T> operator*(T scalar) const { return Vector4(X * scalar, Y * scalar, Z * scalar, W * scalar); }

     Vector4<T> operator*(const Vector4<T>& Other) const { return Vector4(X * Other.X, Y * Other.Y, Z * Other.Z, W * Other.W); }

    Vector4<T>& operator*=(T scalar)
    {
        X *= scalar;
        Y *= scalar;
        Z *= scalar;
        W *= scalar;
        return *this;
    }

    template <typename S>
    Vector4<T> operator/(S scalar) const
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        return Vector4(X * rec, Y * rec, Z * rec, W * rec);
    }

    template <typename S>
    Vector4<T>& operator/=(S scalar)
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        X *= rec;
        Y *= rec;
        Z *= rec;
        W *= rec;
        return *this;
    }

    Vector4<T> operator-() const { return Vector4(-X, -Y, -Z, -W); }

    Vector4<T> Abs() const { return Vector4(std::abs(X), std::abs(Y), std::abs(Z), std::abs(W)); }

    T LengthSquared() const { return X * X + Y * Y + Z * Z + W * W; }
    T Length() const { return std::sqrt(LengthSquared()); }
};

template <typename U, typename T>
inline Vector4<T> operator*(U scalar, const Vector4<T>& v)
{
    return v * (T)scalar;
}

template <typename T>
inline T Dot(const Vector4<T>& v1, const Vector4<T>& v2)
{
    return v1.X * v2.X + v1.Y * v2.Y + v1.Z * v2.Z + v1.W * v2.W;
}

template <typename T>
inline T AbsDot(const Vector4<T>& v1, const Vector4<T>& v2)
{
    return std::abs(Dot(v1, v2));
}

template <typename T>
inline Vector4<T> Normalize(const Vector4<T>& v)
{
    return v / v.Length();
}

template <typename T>
Vector4<T> Min(const Vector4<T>& p1, const Vector4<T>& p2)
{
    return Vector4<T>(std::min(p1.X, p2.X), std::min(p1.Y, p2.Y), std::min(p1.Z, p2.Z), std::min(p1.W, p2.W));
}

template <typename T>
Vector4<T> Max(const Vector4<T>& v1, const Vector4<T>& v2)
{
    return Vector4<T>(std::max(v1.X, v2.X), std::max(v1.Y, v2.Y), std::max(v1.Z, v2.Z), std::max(v1.W, v2.W));
}

template <typename T>
Vector4<T> Permute(const Vector4<T>& v, int X, int Y, int Z, int W)
{
    return Vector4<T>(v[X], v[Y], v[Z], v[W]);
}

}  // namespace rndr