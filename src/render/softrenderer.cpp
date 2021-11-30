#include "rndr/render/softrenderer.h"

#include "rndr/core/bounds3.h"
#include "rndr/core/surface.h"

rndr::SoftwareRenderer::SoftwareRenderer(rndr::Surface* Surface) : m_Surface(Surface) {}

void rndr::SoftwareRenderer::DrawTriangles(const std::vector<Point3r>& Positions,
                                           const std::vector<int>& Indices)
{
    assert(m_Surface);
    assert(m_Pipeline);
    assert(Indices.size() != 0);
    assert(Indices.size() % 3 == 0);

    for (int i = 0; i < Indices.size(); i += 3)
    {
        Point3r Points[3];

        for (int j = 0; j < 3; j++)
        {
            PerVertexInfo VertexInfo;
            VertexInfo.PrimitiveIndex = i / 3;
            VertexInfo.VertexIndex = j;
            VertexInfo.Position = Positions[Indices[i + j]];
            Points[j] = m_Pipeline->VertexShader->Callback(VertexInfo);
        }

        DrawTriangle(Points);
    }
}

static real GetTriangleArea(const rndr::Point3r* Points);
static bool GetBarycentricCoordinates(const rndr::Point3r& Point,
                                      const rndr::Point3r (&Points)[3],
                                      const real OneOverTwoTriangleArea,
                                      const rndr::Vector3r (&Edges)[3],
                                      const rndr::Vector3r& Normal,
                                      real (&BarycentricCoordinates)[3]);

void rndr::SoftwareRenderer::DrawTriangle(const Point3r (&PositionsWithDepth)[3])
{
    // Don't use z coordinate for calculation of barycentric coordinate
    Point3r Points[3] = {{PositionsWithDepth[0].X, PositionsWithDepth[0].Y, 0},
                         {PositionsWithDepth[1].X, PositionsWithDepth[1].Y, 0},
                         {PositionsWithDepth[2].X, PositionsWithDepth[2].Y, 0}};

    Point3r Min = rndr::Min(rndr::Min(Points[0], Points[1]), Points[2]);
    Point3r Max = rndr::Max(rndr::Max(Points[0], Points[1]), Points[2]);
    Min = rndr::Floor(Min);
    Max = rndr::Ceil(Max);

    const Vector3r PixelCenter(0.5, 0.5, 0);

    Min += PixelCenter;
    Max -= PixelCenter;

    Bounds2i TriangleBounds;
    TriangleBounds.pMin = Point2i(Min.X, Min.Y);
    TriangleBounds.pMax = Point2i(Max.X, Max.Y);

    // Early exit if triangle is outside the screen
    if (!rndr::Overlaps(TriangleBounds, m_Surface->GetScreenBounds()))
    {
        return;
    }

    // TODO(mkostic): Check winding order of the triangle and discard it based on that

    Bounds2i TriangleInsideScreen = rndr::Intersect(TriangleBounds, m_Surface->GetScreenBounds());
    Min.X = TriangleInsideScreen.pMin.X;
    Min.Y = TriangleInsideScreen.pMin.Y;
    Max.X = TriangleInsideScreen.pMax.X;
    Max.Y = TriangleInsideScreen.pMax.Y;

    // Stuff unique for triangle
    const real OneOverTwoTriangleArea = 1 / GetTriangleArea(Points) / 2;
    const rndr::Vector3r Edges[3] = {Points[1] - Points[0], Points[2] - Points[1],
                                     Points[0] - Points[2]};
    const rndr::Vector3r N = rndr::Cross(Edges[0], -Edges[2]);

    for (real Y = Min.X; Y <= Max.Y; Y += 1)
    {
        for (real X = Min.X; X <= Max.X; X += 1)
        {
            Point2r Position2D{X, Y};
            Point3r Position{X, Y, 0};
            PerPixelInfo PixelInfo{{(int)X, (int)Y}};
            bool IsInsideTriangle = GetBarycentricCoordinates(
                Position, Points, OneOverTwoTriangleArea, Edges, N, PixelInfo.Barycentric);
            if (IsInsideTriangle)
            {
                rndr::Color Color = m_Pipeline->PixelShader->Callback(PixelInfo);
                Point2i PixelScreen{(int)(Position2D.X - 0.5), (int)(Position2D.Y - 0.5)};

                if (m_Pipeline->bApplyGammaCorrection)
                {
                    Color = Color.ToGammaCorrectSpace(m_Pipeline->Gamma);
                }

                m_Surface->SetPixel(PixelScreen, Color);
            }
        }
    }
}

static bool GetBarycentricCoordinates(const rndr::Point3r& Point,
                                      const rndr::Point3r (&Points)[3],
                                      const real OneOverTwoTriangleArea,
                                      const rndr::Vector3r (&Edges)[3],
                                      const rndr::Vector3r& Normal,
                                      real (&BarycentricCoordinates)[3])
{
    rndr::Vector3r Vec0 = Point - Points[0];
    rndr::Vector3r Vec1 = Point - Points[1];
    rndr::Vector3r Vec2 = Point - Points[2];

    rndr::Vector3r BarVec0 = rndr::Cross(Edges[0], Vec0);
    rndr::Vector3r BarVec1 = rndr::Cross(Edges[1], Vec1);
    rndr::Vector3r BarVec2 = rndr::Cross(Edges[2], Vec2);

    BarycentricCoordinates[0] = rndr::Cross(Edges[0], Vec0).Length() * OneOverTwoTriangleArea;
    BarycentricCoordinates[1] = rndr::Cross(Edges[1], Vec1).Length() * OneOverTwoTriangleArea;
    BarycentricCoordinates[2] = rndr::Cross(Edges[2], Vec2).Length() * OneOverTwoTriangleArea;

    if (rndr::Dot(Normal, BarVec0) < 0)
    {
        return false;
    }

    if (rndr::Dot(Normal, BarVec1) < 0)
    {
        return false;
    }

    if (rndr::Dot(Normal, BarVec2) < 0)
    {
        return false;
    }

    return true;
}

static real GetTriangleArea(const rndr::Point3r* Points)
{
    rndr::Vector3r Edge01 = Points[1] - Points[0];
    rndr::Vector3r Edge02 = Points[2] - Points[0];

    return rndr::Cross(Edge01, Edge02).Length() / 2;
}
