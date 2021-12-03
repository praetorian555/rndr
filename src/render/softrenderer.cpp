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

struct TriangleConstants
{
    rndr::Vector3r Edges[3];
    rndr::Vector3r Normal;
    real OneOverPointDepth[3];
    real OneOverTwoTriangleArea;
};

static real GetTriangleArea(const rndr::Point3r* Points);
static bool GetBarycentricCoordinates(const rndr::Point3r& Point,
                                      const rndr::Point3r (&Points)[3],
                                      const TriangleConstants& Constants,
                                      real (&BarycentricCoordinates)[3]);
static real CalcPixelDepth(const real (&Barycentric)[3], const real (&OneOverDepth)[3]);
static bool RunDepthTest(rndr::DepthTest DepthTest, real SrcDepth, real DestDepth);

// Expecting that PositionsWithDepth are in the real screen space.
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
    TriangleConstants Constants;
    Constants.OneOverTwoTriangleArea = 1 / GetTriangleArea(Points) / 2;
    Constants.Edges[0] = Points[1] - Points[0];
    Constants.Edges[1] = Points[2] - Points[1];
    Constants.Edges[2] = Points[0] - Points[2];
    Constants.OneOverPointDepth[0] = 1 / PositionsWithDepth[0].Z;
    Constants.OneOverPointDepth[1] = 1 / PositionsWithDepth[1].Z;
    Constants.OneOverPointDepth[2] = 1 / PositionsWithDepth[2].Z;
    Constants.Normal = rndr::Cross(Constants.Edges[0], -Constants.Edges[2]);

    for (real Y = Min.X; Y <= Max.Y; Y += 1)
    {
        for (real X = Min.X; X <= Max.X; X += 1)
        {
            Point2r Position2D{X, Y};
            Point3r Position{X, Y, 0};
            PerPixelInfo PixelInfo{{(int)X, (int)Y}};
            bool IsInsideTriangle =
                GetBarycentricCoordinates(Position, Points, Constants, PixelInfo.Barycentric);
            if (IsInsideTriangle)
            {
                Point2i PixelScreen{(int)(Position2D.X - 0.5), (int)(Position2D.Y - 0.5)};

                // Early depth test
                real CandidatePixelDepth =
                    CalcPixelDepth(PixelInfo.Barycentric, Constants.OneOverPointDepth);
                real CurrentPixelDepth = m_Surface->GetPixelDepth(PixelScreen);
                bool DepthTestResult =
                    RunDepthTest(m_Pipeline->DepthTest, CandidatePixelDepth, CurrentPixelDepth);

                if (!DepthTestResult)
                {
                    continue;
                }

                // Depth test success
                m_Surface->SetPixelDepth(PixelScreen, CurrentPixelDepth);

                // Run Pixel shader
                rndr::Color Color = m_Pipeline->PixelShader->Callback(PixelInfo);

                if (m_Pipeline->bApplyGammaCorrection)
                {
                    Color = Color.ToGammaCorrectSpace(m_Pipeline->Gamma);
                }

                // TODO(mkostic): Run standard depth test if user modifies depth in the pixel shader

                // Write color into color buffer
                m_Surface->SetPixel(PixelScreen, Color);
            }
        }
    }
}

static bool GetBarycentricCoordinates(const rndr::Point3r& Point,
                                      const rndr::Point3r (&Points)[3],
                                      const TriangleConstants& Constants,
                                      real (&BarycentricCoordinates)[3])
{
    rndr::Vector3r Vec0 = Point - Points[0];
    rndr::Vector3r Vec1 = Point - Points[1];
    rndr::Vector3r Vec2 = Point - Points[2];

    rndr::Vector3r BarVec0 = rndr::Cross(Constants.Edges[0], Vec0);
    rndr::Vector3r BarVec1 = rndr::Cross(Constants.Edges[1], Vec1);
    rndr::Vector3r BarVec2 = rndr::Cross(Constants.Edges[2], Vec2);

    BarycentricCoordinates[0] =
        rndr::Cross(Constants.Edges[0], Vec0).Length() * Constants.OneOverTwoTriangleArea;
    BarycentricCoordinates[1] =
        rndr::Cross(Constants.Edges[1], Vec1).Length() * Constants.OneOverTwoTriangleArea;
    BarycentricCoordinates[2] =
        rndr::Cross(Constants.Edges[2], Vec2).Length() * Constants.OneOverTwoTriangleArea;

    if (rndr::Dot(Constants.Normal, BarVec0) < 0)
    {
        return false;
    }

    if (rndr::Dot(Constants.Normal, BarVec1) < 0)
    {
        return false;
    }

    if (rndr::Dot(Constants.Normal, BarVec2) < 0)
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

real CalcPixelDepth(const real (&Barycentric)[3], const real (&OneOverDepth)[3])
{
    real Result = OneOverDepth[0] * Barycentric[0] + OneOverDepth[1] * Barycentric[1] +
                  OneOverDepth[2] * Barycentric[2];

    return 1 / Result;
}

bool RunDepthTest(rndr::DepthTest DepthTest, real SrcDepth, real DestDepth)
{
    switch (DepthTest)
    {
        case rndr::DepthTest::GreaterThan:
            return SrcDepth > DestDepth;
        case rndr::DepthTest::LesserThen:
            return SrcDepth < DestDepth;
        case rndr::DepthTest::None:
            return true;
        default:
            assert(false);
    }

    return true;
}
