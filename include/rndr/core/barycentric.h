#pragma once

#include "rndr/core/math.h"

namespace rndr
{

struct BarycentricCoordinates
{
    real X, Y, Z;

    real Interpolate(const real (&Values)[3]) const;
    real Interpolate(real A, real B, real C) const;

    real operator[](int i) const;
};

class BarycentricHelper
{
public:
    BarycentricHelper(rndr::WindingOrder WindingOrder, const rndr::Point3r (&TrianglePoints)[3]);
    BarycentricHelper(rndr::WindingOrder WindingOrder,
                      const rndr::Point3r& TrianglePoint0,
                      const rndr::Point3r& TrianglePoint1,
                      const rndr::Point3r& TrianglePoint2);

    BarycentricCoordinates GetCoordinates(const Point2i& PixelPosition) const;

    bool IsInside(const BarycentricCoordinates& Coordinates) const;

    bool IsWindingOrderCorrect() const;

private:
    void Init();

public:
    rndr::WindingOrder m_WindingOrder;
    rndr::Point3r m_Points[3];
    rndr::Vector3r m_Edges[3];
    real m_OneOverPointDepth[3];
    real m_HalfTriangleArea;
    real m_OneOverHalfTriangleArea;
};

}  // namespace rndr