#pragma once

#include "rndr/core/math.h"

namespace rndr
{

template <typename T>
class Bounds2
{
public:
    Point2<T> pMin, pMax;

public:
    Bounds2()
    {
        T minNum = std::numeric_limits<T>::lowest();
        T maxNum = std::numeric_limits<T>::max();
        pMin = Point2<T>(minNum, minNum);
        pMax = Point2<T>(maxNum, maxNum);
    }

    Bounds2(const Point2<T>& p) : pMin(p), pMax(p) {}

    Bounds2(const Point2<T>& p1, const Point2<T>& p2)
        : pMin(std::min(p1.X, p2.X), std::min(p1.Y, p2.Y)),
          pMax(std::max(p1.X, p2.X), std::max(p1.Y, p2.Y))
    {
    }

    template <typename U>
    Bounds2(const Bounds2<U>& other) : pMin((Point2<T>)other.pMin), pMax((Point2<T>)other.pMax)
    {
    }

    const Point2<T>& operator[](int i) const
    {
        assert(i >= 0 && i < 2);
        if (i == 0)
        {
            return pMin;
        }

        if (i == 1)
        {
            return pMax;
        }
    }

    Point2<T>& operator[](int i)
    {
        assert(i >= 0 && i < 2);
        if (i == 0)
        {
            return pMin;
        }

        if (i == 1)
        {
            return pMax;
        }
    }

    Point2<T> Corner(int corner) const
    {
        return Point2<T>((*this)[(corner & 1)].X, (*this)[(corner & 2) ? 1 : 0].Y);
    }

    Vector2<T> Diagonal() const { return pMax - pMin; }

    T SurfaceArea() const
    {
        Vector2<T> d = Diagonal();
        return d.X * d.Y;
    }

    int MaXimumEXtent() const
    {
        Vector2<T> d = Diagonal();
        return d.X > d.Y ? 0 : 1;
    }

    Point2<T> Lerp(const Point2r& t) const
    {
        return Point2<T>(rndr::Lerp(t.X, pMin.X, pMax.X), rndr::Lerp(t.Y, pMin.Y, pMax.Y));
    }

    Vector2<T> Offset(const Point2<T>& p) const
    {
        Vector2<T> o = p - pMin;
        if (pMax.X > pMin.X)
            o.X /= pMax.X - pMin.X;
        if (pMax.Y > pMin.Y)
            o.Y /= pMax.Y - pMin.Y;
        return o;
    }

    void BoundingSphere(Point2<T>* center, real* radius) const
    {
        *center = (pMin + pMax) / 2;
        *radius = Inside(*center, *this) ? Distance(*center, pMax) : 0;
    }

    Vector2<T> Extent() const
    {
        Vector2<T> Extent{std::abs(pMax.X - pMin.X), std::abs(pMax.Y - pMin.Y)};
        return Extent;
    }
};

template <typename T>
Bounds2<T> Union(const Bounds2<T>& b, const Point2<T>& p)
{
    return Bounds2<T>(Point2<T>(std::min(b.pMin.X, p.X), std::min(b.pMin.Y, p.Y)),
                      Point2<T>(std::max(b.pMax.X, p.X), std::max(b.pMax.Y, p.Y)));
}

template <typename T>
Bounds2<T> Union(const Bounds2<T>& b1, const Bounds2<T>& b2)
{
    return Bounds2<T>(Point2<T>(std::min(b1.pMin.X, b2.pMin.X), std::min(b1.pMin.Y, b2.pMin.Y)),
                      Point2<T>(std::max(b1.pMax.X, b2.pMax.X), std::max(b1.pMax.Y, b2.pMax.Y)));
}

template <typename T>
Bounds2<T> Intersect(const Bounds2<T>& b1, const Bounds2<T>& b2)
{
    return Bounds2<T>(Point2<T>(std::max(b1.pMin.X, b2.pMin.X), std::max(b1.pMin.Y, b2.pMin.Y)),
                      Point2<T>(std::min(b1.pMax.X, b2.pMax.X), std::min(b1.pMax.Y, b2.pMax.Y)));
}

template <typename T>
bool Overlaps(const Bounds2<T>& b1, const Bounds2<T>& b2)
{
    bool X = (b1.pMax.X >= b2.pMin.X) && (b1.pMin.X <= b2.pMax.X);
    bool Y = (b1.pMax.Y >= b2.pMin.Y) && (b1.pMin.Y <= b2.pMax.Y);
    return (X && Y);
}

template <typename T>
bool Inside(const Point2<T>& p, const Bounds2<T>& b)
{
    return (p.X >= b.pMin.X && p.X <= b.pMax.X && p.Y >= b.pMin.Y && p.Y <= b.pMax.Y);
}

// The point is not counted if it is on the upper boundrY of the boX
template <typename T>
bool InsideEXclusive(const Point2<T>& p, const Bounds2<T>& b)
{
    return (p.X >= b.pMin.X && p.X < b.pMax.X && p.Y >= b.pMin.Y && p.Y < b.pMax.Y);
}

template <typename T, typename U>
inline Bounds2<T> EXpand(const Bounds2<T>& b, U delta)
{
    return Bounds2<T>(b.pMin - Vector2<T>(delta, delta), b.pMax + Vector2<T>(delta, delta));
}

// TYpes

using Bounds2r = Bounds2<real>;
using Bounds2i = Bounds2<int>;

}  // namespace rndr