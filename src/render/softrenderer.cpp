#include "rndr/render/softrenderer.h"

#include "rndr/core/bounds3.h"
#include "rndr/core/surface.h"

#include "rndr/render/model.h"

// Helpers ////////////////////////////////////////////////////////////////////////////////////////

struct TriangleConstants
{
    rndr::Vector3r Edges[3];
    rndr::Vector3r Normal;
    real OneOverPointDepth[3];
    real OneOverTriangleArea;
};

static real Edge(const rndr::Vector3r& Edge, const rndr::Vector3r& Vec);
static bool GetBarycentricCoordinates(rndr::WindingOrder WindingOrder,
                                      const rndr::Point3r& Point,
                                      const rndr::Point3r (&Points)[3],
                                      const TriangleConstants& Constants,
                                      real (&BarycentricCoordinates)[3]);
static real CalcPixelDepth(const real (&Barycentric)[3], const real (&OneOverDepth)[3]);
static void GetTriangleBounds(const rndr::Point3r (&Positions)[3], rndr::Bounds3r& TriangleBounds);
static bool IsWindingOrderCorrect(const rndr::Point3r (&Positions)[3],
                                  rndr::WindingOrder WindingOrder);
static bool LimitTriangleToSurface(rndr::Bounds3r& TriangleBounds, const rndr::Surface* Surface);

///////////////////////////////////////////////////////////////////////////////////////////////////

rndr::SoftwareRenderer::SoftwareRenderer(rndr::Surface* Surface) : m_Surface(Surface) {}

void rndr::SoftwareRenderer::Draw(rndr::Model* Model, int InstanceCount)
{
    if (!Model->GetPipeline())
    {
        // TODO(mkostic): Add log
        return;
    }

    m_Pipeline = Model->GetPipeline();

    const std::vector<uint8_t>& VertexData = Model->GetVertexData();
    const int VertexDataStride = Model->GetVertexDataStride();
    const std::vector<uint8_t>& InstanceData = Model->GetInstanceData();
    const int InstanceDataStride = Model->GetInstanceDataStride();
    const std::vector<int> Indices = Model->GetIndices();

    for (int InstanceIndex = 0; InstanceIndex < InstanceCount; InstanceIndex++)
    {
        void* InstanceDataPtr = nullptr;
        if (!InstanceData.empty())
        {
            int Offset = InstanceIndex * InstanceDataStride;
            InstanceDataPtr = (void*)&InstanceData[Offset];
        }
        DrawTriangles(Model->GetConstants(), VertexData, VertexDataStride, Indices,
                      InstanceDataPtr);
    }
}

void rndr::SoftwareRenderer::DrawTriangles(void* Constants,
                                           const std::vector<uint8_t>& VertexData,
                                           int VertexDataStride,
                                           const std::vector<int>& Indices,
                                           void* InstanceData)
{
    assert(m_Surface);
    assert(m_Pipeline);
    assert(Indices.size() != 0);
    assert(Indices.size() % 3 == 0);

    for (int i = 0; i < Indices.size(); i += 3)
    {
        Point3r Positions[3];
        void* Data[3];

        for (int j = 0; j < 3; j++)
        {
            PerVertexInfo VertexInfo;
            VertexInfo.PrimitiveIndex = i / 3;
            VertexInfo.VertexIndex = Indices[i + j];
            VertexInfo.VertexData = (void*)&VertexData.data()[Indices[i + j] * VertexDataStride];
            VertexInfo.InstanceData = InstanceData;
            VertexInfo.Constants = Constants;
            Data[j] = VertexInfo.VertexData;
            Positions[j] = m_Pipeline->VertexShader->Callback(VertexInfo);
            Positions[j] = FromNDCToRasterSpace(Positions[j]);
        }

        DrawTriangle(Constants, Positions, Data);
    }
}

// Positions should be in raster space.
void rndr::SoftwareRenderer::DrawTriangle(void* Constants,
                                          const Point3r (&Positions)[3],
                                          void** VertexData)
{
    Bounds3r TriangleBounds;
    GetTriangleBounds(Positions, TriangleBounds);

    if (!LimitTriangleToSurface(TriangleBounds, m_Surface))
    {
        return;
    }

    if (!IsWindingOrderCorrect(Positions, m_Pipeline->WindingOrder))
    {
        return;
    }

    // Stuff unique for triangle
    TriangleConstants TriConstants;
    const real HalfTriangleArea = Edge(Positions[1] - Positions[0], Positions[2] - Positions[0]);
    TriConstants.OneOverTriangleArea = 1 / HalfTriangleArea;
    TriConstants.Edges[0] = Positions[2] - Positions[1];
    TriConstants.Edges[1] = Positions[0] - Positions[2];
    TriConstants.Edges[2] = Positions[1] - Positions[0];
    TriConstants.OneOverPointDepth[0] = 1 / Positions[0].Z;
    TriConstants.OneOverPointDepth[1] = 1 / Positions[1].Z;
    TriConstants.OneOverPointDepth[2] = 1 / Positions[2].Z;

    // Let's do stuff
    for (real Y = TriangleBounds.pMin.Y; Y <= TriangleBounds.pMax.Y; Y += 1)
    {
        for (real X = TriangleBounds.pMin.X; X <= TriangleBounds.pMax.X; X += 1)
        {
            PerPixelInfo PixelInfo;
            PixelInfo.Position = Point2i{(int)(X - 0.5), (int)(Y - 0.5)};
            PixelInfo.Constants = Constants;
            memcpy(PixelInfo.VertexData, VertexData, sizeof(void*) * 3);

            bool IsInsideTriangle =
                GetBarycentricCoordinates(m_Pipeline->WindingOrder, Point3r{X, Y, 0}, Positions,
                                          TriConstants, PixelInfo.Barycentric);
            if (IsInsideTriangle)
            {
                real NewPixelDepth =
                    CalcPixelDepth(PixelInfo.Barycentric, TriConstants.OneOverPointDepth);

                // Early depth test
                if (!m_Pipeline->PixelShader->bChangesDepth)
                {
                    if (!RunDepthTest(NewPixelDepth, PixelInfo.Position))
                    {
                        continue;
                    }

                    m_Surface->SetPixelDepth(PixelInfo.Position, NewPixelDepth);
                }

                // Run Pixel shader
                rndr::Color Color = m_Pipeline->PixelShader->Callback(PixelInfo, NewPixelDepth);

                // Standard depth test
                if (m_Pipeline->PixelShader->bChangesDepth)
                {
                    if (!RunDepthTest(NewPixelDepth, PixelInfo.Position))
                    {
                        continue;
                    }

                    m_Surface->SetPixelDepth(PixelInfo.Position, NewPixelDepth);
                }

                Color = ApplyAlphaCompositing(Color, PixelInfo.Position);

                if (m_Pipeline->bApplyGammaCorrection)
                {
                    Color = Color.ToGammaCorrectSpace(m_Pipeline->Gamma);
                }

                // Write color into color buffer
                m_Surface->SetPixel(PixelInfo.Position, Color);
            }
        }
    }
}

// Cross product of two vectors but ignoring Z coordinate
static real Edge(const rndr::Vector3r& Edge, const rndr::Vector3r& Vec)
{
    return Edge.X * Vec.Y - Edge.Y * Vec.X;
}

static bool GetBarycentricCoordinates(rndr::WindingOrder WindingOrder,
                                      const rndr::Point3r& Point,
                                      const rndr::Point3r (&Points)[3],
                                      const TriangleConstants& Constants,
                                      real (&Barycentric)[3])
{
    rndr::Vector3r Vec0 = Point - Points[1];
    rndr::Vector3r Vec1 = Point - Points[2];
    rndr::Vector3r Vec2 = Point - Points[0];

    Barycentric[0] =
        Edge(Constants.Edges[0], Vec0) * Constants.OneOverTriangleArea * (int)WindingOrder;
    Barycentric[1] =
        Edge(Constants.Edges[1], Vec1) * Constants.OneOverTriangleArea * (int)WindingOrder;
    Barycentric[2] =
        Edge(Constants.Edges[2], Vec2) * Constants.OneOverTriangleArea * (int)WindingOrder;

    if (Barycentric[0] < 0 || Barycentric[1] < 0 || Barycentric[2] < 0)
    {
        return false;
    }

    bool Return = true;
    Return &= Barycentric[0] == 0
                  ? ((Constants.Edges[0].Y == 0 && (int)WindingOrder * Constants.Edges[0].X < 0) ||
                     ((int)WindingOrder * Constants.Edges[0].Y < 0))
                  : true;
    Return &= Barycentric[1] == 0
                  ? ((Constants.Edges[1].Y == 0 && (int)WindingOrder * Constants.Edges[1].X < 0) ||
                     ((int)WindingOrder * Constants.Edges[1].Y < 0))
                  : true;
    Return &= Barycentric[2] == 0
                  ? ((Constants.Edges[2].Y == 0 && (int)WindingOrder * Constants.Edges[2].X < 0) ||
                     ((int)WindingOrder * Constants.Edges[2].Y < 0))
                  : true;

    return Return;
}

static real CalcPixelDepth(const real (&Barycentric)[3], const real (&OneOverDepth)[3])
{
    real Result = OneOverDepth[0] * Barycentric[0] + OneOverDepth[1] * Barycentric[1] +
                  OneOverDepth[2] * Barycentric[2];

    return 1 / Result;
}

static void GetTriangleBounds(const rndr::Point3r (&Positions)[3], rndr::Bounds3r& TriangleBounds)
{
    rndr::Point3r Min = rndr::Min(rndr::Min(Positions[0], Positions[1]), Positions[2]);
    rndr::Point3r Max = rndr::Max(rndr::Max(Positions[0], Positions[1]), Positions[2]);
    Min = rndr::Floor(Min);
    Max = rndr::Ceil(Max);

    const rndr::Vector3r PixelCenterOffset(0.5, 0.5, 0);

    Min += PixelCenterOffset;
    Max -= PixelCenterOffset;

    TriangleBounds.pMin = Min;
    TriangleBounds.pMax = Max;
}

static bool IsWindingOrderCorrect(const rndr::Point3r (&Positions)[3],
                                  rndr::WindingOrder WindingOrder)
{
    real HalfTriangleArea = Edge(Positions[1] - Positions[0], Positions[2] - Positions[0]);
    HalfTriangleArea *= (int)WindingOrder;
    return HalfTriangleArea >= 0;
}

static bool LimitTriangleToSurface(rndr::Bounds3r& TriangleBounds, const rndr::Surface* Surface)
{
    rndr::Bounds3r ScreenBounds = Surface->GetScreenBounds();
    ScreenBounds.pMin += rndr::Point3r{0.5, 0.5, 0};
    ScreenBounds.pMax += rndr::Point3r{-0.5, -0.5, 0};

    if (!rndr::Overlaps(TriangleBounds, ScreenBounds))
    {
        return false;
    }

    TriangleBounds = rndr::Intersect(TriangleBounds, ScreenBounds);

    return true;
}

bool rndr::SoftwareRenderer::RunDepthTest(real NewDepthValue, const Point2i& PixelPosition)
{
    if (NewDepthValue < 0 || NewDepthValue > 1)
    {
        return false;
    }

    const real DestDepth = m_Surface->GetPixelDepth(PixelPosition);

    switch (m_Pipeline->DepthTest)
    {
        case rndr::DepthTest::GreaterThan:
            return NewDepthValue > DestDepth;
        case rndr::DepthTest::LesserThen:
            return NewDepthValue < DestDepth;
        case rndr::DepthTest::None:
            return true;
        default:
            assert(false);
    }

    return true;
}

rndr::Color rndr::SoftwareRenderer::ApplyAlphaCompositing(Color NewValue,
                                                          const Point2i& PixelPosition)
{
    rndr::Color CurrentColor = m_Surface->GetPixelColor(PixelPosition);
    CurrentColor = CurrentColor.ToLinearSpace(m_Pipeline->Gamma);
    real InvColorA = 1 - NewValue.A;

    NewValue.R = NewValue.A * NewValue.R + CurrentColor.R * CurrentColor.A * InvColorA;
    NewValue.G = NewValue.A * NewValue.G + CurrentColor.G * CurrentColor.A * InvColorA;
    NewValue.B = NewValue.A * NewValue.B + CurrentColor.B * CurrentColor.A * InvColorA;
    NewValue.A = NewValue.A + CurrentColor.A * InvColorA;

    return NewValue;
}

rndr::Point3r rndr::SoftwareRenderer::FromNDCToRasterSpace(const Point3r& Point)
{
    int Width = m_Surface->GetWidth();
    int Height = m_Surface->GetHeight();

    Point3r Result = Point;
    Result.X = ((1 + Point.X) / 2) * Width;
    Result.Y = ((1 + Point.Y) / 2) * Height;

    return Result;
}

rndr::Point3r rndr::SoftwareRenderer::FromRasterToNDCSpace(const Point3r& Point)
{
    int Width = m_Surface->GetWidth();
    int Height = m_Surface->GetHeight();

    Point3r Result = Point;
    Result.X = (Point.X / (real)Width) * 2 - 1;
    Result.Y = (Point.Y / (real)Height) * 2 - 1;

    return Result;
}
