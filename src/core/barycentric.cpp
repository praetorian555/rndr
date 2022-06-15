#include "rndr/core/barycentric.h"

#include "math/point2.h"

#include "rndr/core/coordinates.h"
#include "rndr/core/debug.h"

real rndr::BarycentricCoordinates::Interpolate(const real (&Values)[3]) const
{
    return X * Values[0] + Y * Values[1] + Z * Values[2];
}

real rndr::BarycentricCoordinates::Interpolate(real A, real B, real C) const
{
    return X * A + Y * B + Z * C;
}

real rndr::BarycentricCoordinates::operator[](int i) const
{
    switch (i)
    {
        case 0:
            return X;
        case 1:
            return Y;
        case 2:
            return Z;
        default:
            assert(false);
    }

    return X;
}

rndr::BarycentricHelper::BarycentricHelper(WindingOrder FrontFaceWindingOrder, const math::Point3 (&TrianglePoints)[3])
{
    m_FrontFaceWindingOrder = FrontFaceWindingOrder;

    memcpy(m_Points, TrianglePoints, sizeof(TrianglePoints));
    Init();
}

rndr::BarycentricHelper::BarycentricHelper(WindingOrder FrontFaceWindingOrder,
                                           const math::Point3& TrianglePoint0,
                                           const math::Point3& TrianglePoint1,
                                           const math::Point3& TrianglePoint2)
{
    m_FrontFaceWindingOrder = FrontFaceWindingOrder;

    m_Points[0] = TrianglePoint0;
    m_Points[1] = TrianglePoint1;
    m_Points[2] = TrianglePoint2;

    Init();
}

void rndr::BarycentricHelper::Init()
{
    m_HalfTriangleArea = Cross2D(m_Points[1] - m_Points[0], m_Points[2] - m_Points[0]);
    m_HalfTriangleArea = std::abs(m_HalfTriangleArea);
    m_OneOverHalfTriangleArea = 1 / m_HalfTriangleArea;
    assert(!math::IsNaN(m_OneOverHalfTriangleArea));
    m_Edges[0] = m_Points[2] - m_Points[1];
    m_Edges[1] = m_Points[0] - m_Points[2];
    m_Edges[2] = m_Points[1] - m_Points[0];
}

rndr::BarycentricCoordinates rndr::BarycentricHelper::GetCoordinates(const math::Point2& PixelPosition) const
{
    // NOTE(mkostic): Since we are using right-handed coordinate system and, hence, right-handed rule for the cross product, then when
    // points are in counter-clockwise order we get positive results if the pixel position is inside the triangle. When we are using
    // clockwise order of points we multiply the coordinates with negative one to keep them positive.

    const math::Point3 Point = rndr::PixelCoordinates::ToContinuousSpace((math::Point3)PixelPosition);

    math::Vector3 Vec0;
    math::Vector3 Vec1;
    math::Vector3 Vec2;
    Vec0 = Point - m_Points[1];
    Vec1 = Point - m_Points[2];
    Vec2 = Point - m_Points[0];

    const int Multiplier = m_FrontFaceWindingOrder == WindingOrder::CCW ? 1 : -1;
    BarycentricCoordinates BarCoords;
    BarCoords.X = Cross2D(m_Edges[0], Vec0) * m_OneOverHalfTriangleArea * Multiplier;
    BarCoords.Y = Cross2D(m_Edges[1], Vec1) * m_OneOverHalfTriangleArea * Multiplier;
    BarCoords.Z = Cross2D(m_Edges[2], Vec2) * m_OneOverHalfTriangleArea * Multiplier;

    return BarCoords;
}

bool rndr::BarycentricHelper::IsInside(const BarycentricCoordinates& Coords) const
{
    // NOTE(mkostic): In the case of clockwise winding order we need to adjust the sign of the edges.

    if (Coords.X < 0 || Coords.Y < 0 || Coords.Z < 0)
    {
        return false;
    }

    const int Multiplier = m_FrontFaceWindingOrder == WindingOrder::CCW ? 1 : -1;
    bool Return = true;
    Return &= Coords.X == 0 ? ((m_Edges[0].Y == 0 && Multiplier * m_Edges[0].X < 0) || (Multiplier * m_Edges[0].Y < 0)) : true;
    Return &= Coords.Y == 0 ? ((m_Edges[1].Y == 0 && Multiplier * m_Edges[1].X < 0) || (Multiplier * m_Edges[1].Y < 0)) : true;
    Return &= Coords.Z == 0 ? ((m_Edges[2].Y == 0 && Multiplier * m_Edges[2].X < 0) || (Multiplier * m_Edges[2].Y < 0)) : true;

    return Return;
}
