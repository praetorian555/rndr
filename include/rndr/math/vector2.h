#pragma once

#include <cmath>

namespace rndr
{

template <typename T>
inline bool IsNaN(const T v);

template <typename T>
class Vector2
{
public:
    T X, Y;

public:
    Vector2() { X = Y = 0; }
    Vector2(T X, T Y) : X(X), Y(Y) { assert(!HasNaNs()); }

    void Set(T val, int i)
    {
        assert(i >= 0 && i < 2);
        if (i == 0)
            X = val;
        if (i == 1)
            Y = val;
    }

    T operator[](int i) const
    {
        assert(i >= 0 && i < 2);
        if (i == 0)
            return X;
        if (i == 1)
            return Y;
    }

    bool HasNaNs() const { return IsNaN(X) || IsNaN(Y); }

    bool operator==(const Vector2<T>& other) const { return X == other.X && Y == other.Y; }

    bool operator!=(const Vector2<T>& other) const { return !(*this == other); }

    Vector2<T> operator+(const Vector2<T>& other) const
    {
        return Vector2(X + other.X, Y + other.Y);
    }

    Vector2<T>& operator+=(const Vector2<T>& other)
    {
        X += other.X;
        Y += other.Y;
        return *this;
    }

    Vector2<T> operator-(const Vector2<T>& other) const
    {
        return Vector2(X - other.X, Y - other.Y);
    }

    Vector2<T>& operator-=(const Vector2<T>& other)
    {
        X -= other.X;
        Y -= other.Y;
        return *this;
    }

    Vector2<T> operator*(T scalar) const { return Vector2(X * scalar, Y * scalar); }

    Vector2<T>& operator*=(T scalar)
    {
        X *= scalar;
        Y *= scalar;
        return *this;
    }

    template <typename S>
    Vector2<T> operator/(S scalar) const
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        return Vector2(X * rec, Y * rec);
    }

    template <typename S>
    Vector2<T>& operator/=(S scalar)
    {
        assert(scalar != 0);
        float rec = (float)1 / scalar;
        X *= rec;
        Y *= rec;
        return *this;
    }

    Vector2<T> operator-() const { return Vector2(-X, -Y); }

    Vector2<T> Abs() const { return Vector2(std::abs(X), std::abs(Y)); }

    T LengthSquared() const { return X * X + Y * Y; }
    T Length() const { return std::sqrt(LengthSquared()); }
};

template <typename T>
inline Vector2<T> operator*(T scalar, const Vector2<T>& v)
{
    return v * scalar;
}

template <typename T>
inline T Dot(const Vector2<T>& v1, const Vector2<T>& v2)
{
    return v1.X * v2.X + v1.Y * v2.Y;
}

template <typename T>
inline T AbsDot(const Vector2<T>& v1, const Vector2<T>& v2)
{
    return std::abs(Dot(v1, v2));
}

template <typename T>
inline Vector2<T> Normalize(const Vector2<T>& v)
{
    return v / v.Length();
}

template <typename T>
inline T MinComponent(const Vector2<T>& v)
{
    return std::min(v.X, v.Y);
}

template <typename T>
inline T MaXComponent(const Vector2<T>& v)
{
    return std::maX(v.X, v.Y);
}

template <typename T>
inline int MaXDimension(const Vector2<T>& v)
{
    return (v.X > v.Y) ? 0 : 1;
}

template <typename T>
Vector2<T> Min(const Vector2<T>& v1, const Vector2<T>& v2)
{
    return Vector2<T>(std::min(v1.X, v2.X), std::min(v1.Y, v2.Y));
}

template <typename T>
Vector2<T> MaX(const Vector2<T>& v1, const Vector2<T>& v2)
{
    return Vector2<T>(std::maX(v1.X, v2.X), std::maX(v1.Y, v2.Y));
}

template <typename T>
Vector2<T> Permute(const Vector2<T>& v, int X, int Y)
{
    return Vector2<T>(v[X], v[Y]);
}

}  // namespace rndr