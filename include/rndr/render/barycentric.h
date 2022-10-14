#pragma once

#include "math/point3.h"
#include "math/vector3.h"

#include "rndr/render/pipeline.h"

// Forward declarations
struct math::Point2;

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
    BarycentricHelper(WindingOrder FrontFaceWindingOrder, const math::Point3 (&TrianglePoints)[3]);
    BarycentricHelper(WindingOrder FrontFaceWindingOrder,
                      const math::Point3& TrianglePoint0,
                      const math::Point3& TrianglePoint1,
                      const math::Point3& TrianglePoint2);

    BarycentricCoordinates GetCoordinates(const math::Point2& PixelPosition) const;
    bool IsInside(const BarycentricCoordinates& Coordinates) const;

private:
    void Init();

public:
    WindingOrder m_FrontFaceWindingOrder;
    math::Point3 m_Points[3];
    math::Vector3 m_Edges[3];
    real m_HalfTriangleArea;
    real m_OneOverHalfTriangleArea;
};

}  // namespace rndr
