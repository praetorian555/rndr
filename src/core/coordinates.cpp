#include "rndr/core/coordinates.h"

rndr::Point2i rndr::PixelCoordinates::ToDiscreteSpace(const Point2r& Point)
{
    return Point2i{(int)std::floor(Point.X), (int)std::floor(Point.Y)};
}

rndr::Point3i rndr::PixelCoordinates::ToDiscreteSpace(const Point3r& Point)
{
    return Point3i{(int)std::floor(Point.X), (int)std::floor(Point.Y), (int)std::floor(Point.Z)};
}

rndr::Point2r rndr::PixelCoordinates::ToContinuousSpace(const Point2i& Point)
{
    return Point2r{(real)Point.X + (real)0.5, (real)Point.Y + (real)0.5};
}

rndr::Point3r rndr::PixelCoordinates::ToContinuousSpace(const Point3i& Point)
{
    return Point3r{(real)Point.X + (real)0.5, (real)Point.Y + (real)0.5, (real)Point.Z + (real)0.5};
}