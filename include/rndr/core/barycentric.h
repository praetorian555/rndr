#pragma once

#include "rndr/core/math.h"
#include "rndr/core/pipeline.h"

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
    BarycentricHelper() = default;
    BarycentricHelper(WindingOrder FrontFaceWindingOrder, const rndr::Point3r (&TrianglePoints)[3]);
    BarycentricHelper(WindingOrder FrontFaceWindingOrder,
                      const Point3r& TrianglePoint0,
                      const Point3r& TrianglePoint1,
                      const Point3r& TrianglePoint2);

    BarycentricCoordinates GetCoordinates(const Point2i& PixelPosition) const;
    bool IsInside(const BarycentricCoordinates& Coordinates) const;

private:
    void Init();

public:
    WindingOrder m_FrontFaceWindingOrder;
    Point3r m_Points[3];
    Vector3r m_Edges[3];
    real m_HalfTriangleArea;
    real m_OneOverHalfTriangleArea;
};

}  // namespace rndr