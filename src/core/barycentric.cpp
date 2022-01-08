#include "rndr/core/barycentric.h"

#include "rndr/core/coordinates.h"

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

rndr::BarycentricHelper::BarycentricHelper(rndr::WindingOrder WindingOrder,
                                           const rndr::Point3r (&TrianglePoints)[3])
{
    m_WindingOrder = WindingOrder;

    memcpy(m_Points, TrianglePoints, sizeof(TrianglePoints));
    Init();
}

rndr::BarycentricHelper::BarycentricHelper(rndr::WindingOrder WindingOrder,
                                           const rndr::Point3r& TrianglePoint0,
                                           const rndr::Point3r& TrianglePoint1,
                                           const rndr::Point3r& TrianglePoint2)
{
    m_WindingOrder = WindingOrder;

    m_Points[0] = TrianglePoint0;
    m_Points[1] = TrianglePoint1;
    m_Points[2] = TrianglePoint2;

    Init();
}

void rndr::BarycentricHelper::Init()
{
    m_HalfTriangleArea = Cross2D(m_Points[1] - m_Points[0], m_Points[2] - m_Points[0]);
    m_OneOverHalfTriangleArea = 1 / m_HalfTriangleArea;
    m_Edges[0] = m_Points[2] - m_Points[1];
    m_Edges[1] = m_Points[0] - m_Points[2];
    m_Edges[2] = m_Points[1] - m_Points[0];
    m_OneOverPointDepth[0] = 1 / m_Points[0].Z;
    m_OneOverPointDepth[1] = 1 / m_Points[1].Z;
    m_OneOverPointDepth[2] = 1 / m_Points[2].Z;
}

bool rndr::BarycentricHelper::IsWindingOrderCorrect() const
{
    const real HalfTriangleArea = m_HalfTriangleArea * (int)m_WindingOrder;
    return HalfTriangleArea >= 0;
}

rndr::BarycentricCoordinates rndr::BarycentricHelper::GetCoordinates(
    const Point2i& PixelPosition) const
{
    const rndr::Point3r Point =
        rndr::PixelCoordinates::ToContinuousSpace((rndr::Point3i)PixelPosition);

    const rndr::Vector3r Vec0 = Point - m_Points[1];
    const rndr::Vector3r Vec1 = Point - m_Points[2];
    const rndr::Vector3r Vec2 = Point - m_Points[0];

    BarycentricCoordinates BarCoords;
    BarCoords.X = Cross2D(m_Edges[0], Vec0) * m_OneOverHalfTriangleArea * (int)m_WindingOrder;
    BarCoords.Y = Cross2D(m_Edges[1], Vec1) * m_OneOverHalfTriangleArea * (int)m_WindingOrder;
    BarCoords.Z = Cross2D(m_Edges[2], Vec2) * m_OneOverHalfTriangleArea * (int)m_WindingOrder;

    return BarCoords;
}

bool rndr::BarycentricHelper::IsInside(const BarycentricCoordinates& Coords) const
{
    if (Coords.X < 0 || Coords.Y < 0 || Coords.Z < 0)
    {
        return false;
    }

    bool Return = true;
    Return &= Coords.X == 0 ? ((m_Edges[0].Y == 0 && (int)m_WindingOrder * m_Edges[0].X < 0) ||
                               ((int)m_WindingOrder * m_Edges[0].Y < 0))
                            : true;
    Return &= Coords.Y == 0 ? ((m_Edges[1].Y == 0 && (int)m_WindingOrder * m_Edges[1].X < 0) ||
                               ((int)m_WindingOrder * m_Edges[1].Y < 0))
                            : true;
    Return &= Coords.Z == 0 ? ((m_Edges[2].Y == 0 && (int)m_WindingOrder * m_Edges[2].X < 0) ||
                               ((int)m_WindingOrder * m_Edges[2].Y < 0))
                            : true;

    return Return;
}