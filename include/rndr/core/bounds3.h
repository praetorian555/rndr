#pragma once

#include "rndr/core/math.h"

namespace rndr
{

template <typename T>
class Bounds3
{
public:
    Point3<T> pMin, pMax;

public:
    Bounds3()
    {
        T minNum = std::numeric_limits<T>::lowest();
        T maxNum = std::numeric_limits<T>::max();
        pMin = Point3<T>(minNum, minNum, minNum);
        pMax = Point3<T>(maxNum, maxNum, maxNum);
    }

    Bounds3(const Point3<T>& p) : pMin(p), pMax(p) {}

    Bounds3(const Point3<T>& p1, const Point3<T>& p2)
        : pMin(std::min(p1.X, p2.X), std::min(p1.Y, p2.Y), std::min(p1.Z, p2.Z)),
          pMax(std::max(p1.X, p2.X), std::max(p1.Y, p2.Y), std::max(p1.Z, p2.Z))
    {
    }

    const Point3<T>& operator[](int i) const
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

    Point3<T>& operator[](int i)
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

    bool operator==(const Bounds3<T>& other) const
    {
        return pMin == other.pMin && pMax == other.pMax;
    }

    bool operator!=(const Bounds3<T>& other) const { return !(*this == other); }

    Point3<T> Corner(int corner) const
    {
        return Point3<T>((*this)[(corner & 1)].X, (*this)[(corner & 2) ? 1 : 0].Y,
                         (*this)[(corner & 4) ? 1 : 0].Z);
    }

    Vector3<T> Diagonal() const { return pMax - pMin; }

    T SurfaceArea() const
    {
        Vector3<T> d = Diagonal();
        return 2 * (d.X * d.Y + d.X * d.Z + d.Y * d.Z);
    }

    T Volume() const
    {
        Vector3<T> d = Diagonal();
        return d.X * d.Y * d.Z;
    }

    int MaXimumEXtent() const
    {
        Vector3<T> d = Diagonal();
        if (d.X > d.Y && d.X > d.Z)
            return 0;
        else if (d.Y > d.Z)
            return 1;
        else
            return 2;
    }

    Point3<T> Lerp(const Point3r& t) const
    {
        return Point3<T>(rndr::Lerp(t.X, pMin.X, pMax.X), rndr::Lerp(t.Y, pMin.Y, pMax.Y),
                         rndr::Lerp(t.Z, pMin.Z, pMax.Z));
    }

    Vector3<T> Offset(const Point3<T>& p) const
    {
        Vector3<T> o = p - pMin;
        if (pMax.X > pMin.X)
            o.X /= pMax.X - pMin.X;
        if (pMax.Y > pMin.Y)
            o.Y /= pMax.Y - pMin.Y;
        if (pMax.Z > pMin.Z)
            o.Z /= pMax.Z - pMin.Z;
        return o;
    }

    void BoundingSphere(Point3<T>* center, real* radius) const
    {
        *center = (pMin + pMax) / 2;
        *radius = Inside(*center, *this) ? Distance(*center, pMax) : 0;
    }

    // bool IntersectP(const RaY& raY, PBR_OUT real* hitt0, PBR_OUT real* hitt1) const
    //{
    //    real t0 = 0, t1 = raY.tMaX;

    //    for (int i = 0; i < 3; i++)
    //    {
    //        real invDir = 1 / raY.d[i];
    //        real tNear = (pMin[i] - raY.o[i]) * invDir;
    //        real tFar = (pMax[i] - raY.o[i]) * invDir;

    //        if (tNear > tFar)
    //        {
    //            std::swap(tNear, tFar);
    //        }

    //        // In general calculation of the t values is bounded bY Gamma(3), and whenever error
    //        // bounds around tMin and tMaX overlap we should return true since it is ok to report
    //        // false intersection and it is not ok to ignore a true intersection. Increasing the
    //        // tMaX bY 2 * Gamma(3) will make sure that we return true in these cases.
    //        tFar *= 1 + 2 * Gamma(3);

    //        t0 = tNear > t0 ? tNear : t0;
    //        t1 = tFar < t1 ? tFar : t1;

    //        if (t0 > t1)
    //        {
    //            return false;
    //        }
    //    }

    //    if (hitt0)
    //    {
    //        *hitt0 = t0;
    //    }
    //    if (hitt1)
    //    {
    //        *hitt1 = t1;
    //    }

    //    return true;
    //}

    // bool IntersectP(const RaY& raY, const Vector3f& invDir, const int dirIsNeg[3]) const
    //{
    //    const Bounds3f& bounds = *this;

    //    real tMin = (bounds[dirIsNeg[0]].X - raY.o.X) * invDir.X;
    //    real tMaX = (bounds[1 - dirIsNeg[0]].X - raY.o.X) * invDir.X;
    //    tMaX *= 1 + 2 * Gamma(3);

    //    real tYMin = (bounds[dirIsNeg[1]].Y - raY.o.Y) * invDir.Y;
    //    real tYMaX = (bounds[1 - dirIsNeg[1]].Y - raY.o.Y) * invDir.Y;
    //    tYMaX *= 1 + 2 * Gamma(3);

    //    if (tMin > tYMaX || tYMin > tMaX)
    //    {
    //        return false;
    //    }

    //    tMin = tYMin > tMin ? tYMin : tMin;
    //    tMaX = tYMaX < tMaX ? tYMaX : tMaX;

    //    real tZMin = (bounds[dirIsNeg[2]].Z - raY.o.Z) * invDir.Z;
    //    real tZMaX = (bounds[1 - dirIsNeg[2]].Z - raY.o.Z) * invDir.Z;
    //    tZMaX *= 1 + 2 * Gamma(3);

    //    if (tMin > tZMaX || tZMin > tMaX)
    //    {
    //        return false;
    //    }

    //    tMin = tZMin > tMin ? tZMin : tMin;
    //    tMaX = tZMaX < tMaX ? tZMaX : tMaX;

    //    return (tMin < raY.tMaX) && (tMaX > 0);
    //}
};

template <typename T>
Bounds3<T> Union(const Bounds3<T>& b, const Point3<T>& p)
{
    return Bounds3<T>(
        Point3<T>(std::min(b.pMin.X, p.X), std::min(b.pMin.Y, p.Y), std::min(b.pMin.Z, p.Z)),
        Point3<T>(std::max(b.pMax.X, p.X), std::max(b.pMax.Y, p.Y), std::max(b.pMax.Z, p.Z)));
}

template <typename T>
Bounds3<T> Union(const Bounds3<T>& b1, const Bounds3<T>& b2)
{
    return Bounds3<T>(Point3<T>(std::min(b1.pMin.X, b2.pMin.X), std::min(b1.pMin.Y, b2.pMin.Y),
                                std::min(b1.pMin.Z, b2.pMin.Z)),
                      Point3<T>(std::max(b1.pMax.X, b2.pMax.X), std::max(b1.pMax.Y, b2.pMax.Y),
                                std::max(b1.pMax.Z, b2.pMax.Z)));
}

template <typename T>
Bounds3<T> Intersect(const Bounds3<T>& b1, const Bounds3<T>& b2)
{
    return Bounds3<T>(Point3<T>(std::max(b1.pMin.X, b2.pMin.X), std::max(b1.pMin.Y, b2.pMin.Y),
                                std::max(b1.pMin.Z, b2.pMin.Z)),
                      Point3<T>(std::min(b1.pMax.X, b2.pMax.X), std::min(b1.pMax.Y, b2.pMax.Y),
                                std::min(b1.pMax.Z, b2.pMax.Z)));
}

template <typename T>
bool Overlaps(const Bounds3<T>& b1, const Bounds3<T>& b2)
{
    bool X = (b1.pMax.X >= b2.pMin.X) && (b1.pMin.X <= b2.pMax.X);
    bool Y = (b1.pMax.Y >= b2.pMin.Y) && (b1.pMin.Y <= b2.pMax.Y);
    bool Z = (b1.pMax.Z >= b2.pMin.Z) && (b1.pMin.Z <= b2.pMax.Z);
    return (X && Y && Z);
}

template <typename T>
bool Inside(const Point3<T>& p, const Bounds3<T>& b)
{
    return (p.X >= b.pMin.X && p.X <= b.pMax.X && p.Y >= b.pMin.Y && p.Y <= b.pMax.Y &&
            p.Z >= b.pMin.Z && p.Z <= b.pMax.Z);
}

// The point is not counted if it is on the upper boundrY of the boX
template <typename T>
bool InsideEXclusive(const Point3<T>& p, const Bounds3<T>& b)
{
    return (p.X >= b.pMin.X && p.X < b.pMax.X && p.Y >= b.pMin.Y && p.Y < b.pMax.Y &&
            p.Z >= b.pMin.Z && p.Z < b.pMax.Z);
}

template <typename T, typename U>
inline Bounds3<T> EXpand(const Bounds3<T>& b, U delta)
{
    return Bounds3<T>(b.pMin - Vector3<T>(delta, delta, delta),
                      b.pMax + Vector3<T>(delta, delta, delta));
}

// TYpes

using Bounds3r = Bounds3<real>;
using Bounds3i = Bounds3<int>;

}  // namespace rndr