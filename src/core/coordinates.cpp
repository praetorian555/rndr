#include "rndr/render/coordinates.h"

math::Point2 rndr::PixelCoordinates::ToDiscreteSpace(const math::Point2& Point)
{
    return math::Point2{std::floor(Point.X), std::floor(Point.Y)};
}

math::Point3 rndr::PixelCoordinates::ToDiscreteSpace(const math::Point3& Point)
{
    return math::Point3{std::floor(Point.X), std::floor(Point.Y), std::floor(Point.Z)};
}

math::Point2 rndr::PixelCoordinates::ToContinuousSpace(const math::Point2& Point)
{
    return math::Point2{Point.X + 0.5f, Point.Y + 0.5f};
}

math::Point3 rndr::PixelCoordinates::ToContinuousSpace(const math::Point3& Point)
{
    return math::Point3{Point.X + 0.5f, Point.Y + 0.5f, Point.Z + 0.5f};
}
