#include "rndr/render/rasterizer.h"

#include "rndr/core/barycentric.h"
#include "rndr/core/bounds3.h"
#include "rndr/core/coordinates.h"
#include "rndr/core/threading.h"

#include "rndr/render/image.h"
#include "rndr/render/model.h"

// Helpers ////////////////////////////////////////////////////////////////////////////////////////

struct TriangleConstants
{
    rndr::Vector3r Edges[3];
    rndr::Vector3r Normal;
    real OneOverPointDepth[3];
    real OneOverTriangleArea;
};

static void GetTriangleBounds(const rndr::Point3r (&Positions)[3], rndr::Bounds2i& TriangleBounds);
static bool LimitTriangleToSurface(rndr::Bounds2i& TriangleBounds, const rndr::Image* Image);
static bool ShouldDiscardByDepth(const rndr::Point3r (&Points)[3]);

///////////////////////////////////////////////////////////////////////////////////////////////////

void rndr::Rasterizer::Draw(rndr::Model* Model, int InstanceCount)
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

void rndr::Rasterizer::DrawTriangles(void* Constants,
                                     const std::vector<uint8_t>& VertexData,
                                     int VertexDataStride,
                                     const std::vector<int>& Indices,
                                     void* InstanceData)
{
    assert(m_Pipeline);
    assert(Indices.size() != 0);
    assert(Indices.size() % 3 == 0);

    const int EndIndex = Indices.size();
    const int StartIndex = 0;
    const int BatchSize = 1;
    const int StepSize = 3;

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
void rndr::Rasterizer::DrawTriangle(void* Constants,
                                    const Point3r (&Positions)[3],
                                    void** VertexData)
{
    Bounds2i TriangleBounds;
    GetTriangleBounds(Positions, TriangleBounds);

    if (!LimitTriangleToSurface(TriangleBounds, m_Pipeline->ColorImage))
    {
        return;
    }

    if (ShouldDiscardByDepth(Positions))
    {
        return;
    }

    const BarycentricHelper BarHelper(m_Pipeline->WindingOrder, Positions);

    if (!BarHelper.IsWindingOrderCorrect())
    {
        return;
    }

    // Add 1 for easier iteration
    TriangleBounds.pMax += 1;

    const Vector2i TriangleSize = TriangleBounds.Diagonal();

    // Initialize Pixel infos
    std::vector<PerPixelInfo> PixelInfos(TriangleBounds.SurfaceArea());
    for (int Y = 0; Y < TriangleSize.Y; Y++)
    {
        for (int X = 0; X < TriangleSize.X; X++)
        {
            const Point2i PixelOffset{X, Y};

            const int CurrentIndex = X + Y * TriangleSize.X;
            const int NextXIndex =
                (X == TriangleSize.X - 1) ? Y * TriangleSize.X : CurrentIndex + 1;
            const int NextYIndex = (Y == TriangleSize.Y - 1) ? X : X + (Y + 1) * TriangleSize.X;
            PerPixelInfo& PixelInfo = PixelInfos[CurrentIndex];
            PixelInfo.NextX = &PixelInfos[NextXIndex];
            PixelInfo.NextY = &PixelInfos[NextYIndex];
            PixelInfo.Position = TriangleBounds.pMin + PixelOffset;
            PixelInfo.Constants = Constants;
            PixelInfo.BarCoords = BarHelper.GetCoordinates(TriangleBounds.pMin + PixelOffset);
            memcpy(PixelInfo.VertexData, VertexData, sizeof(void*) * 3);
        }
    }

    // X and Y are in discrete space, do depth test and run pixel shader
    for (int Y = 0; Y < TriangleSize.Y; Y++)
    {
        for (int X = 0; X < TriangleSize.X; X++)
        {
            const Point2i PixelPositionOffset{X, Y};
            const Point2i PixelPosition = TriangleBounds.pMin + PixelPositionOffset;

            const int PixelIndex = X + Y * TriangleSize.X;
            PerPixelInfo& PixelInfo = PixelInfos[PixelIndex];

            if (BarHelper.IsInside(PixelInfo.BarCoords))
            {
                real NewPixelDepth =
                    1 / PixelInfo.BarCoords.Interpolate(BarHelper.m_OneOverPointDepth);

                // Early depth test
                if (!m_Pipeline->PixelShader->bChangesDepth)
                {
                    if (!RunDepthTest(PixelPosition, NewPixelDepth))
                    {
                        continue;
                    }

                    m_Pipeline->DepthImage->SetPixelDepth(PixelPosition, NewPixelDepth);
                }

                // Run Pixel shader
                rndr::Color Color = m_Pipeline->PixelShader->Callback(PixelInfo, NewPixelDepth);

                // Standard depth test
                if (m_Pipeline->PixelShader->bChangesDepth)
                {
                    if (!RunDepthTest(PixelPosition, NewPixelDepth))
                    {
                        continue;
                    }

                    m_Pipeline->DepthImage->SetPixelDepth(PixelPosition, NewPixelDepth);
                }

                Color = ApplyAlphaCompositing(PixelPosition, Color);

                if (m_Pipeline->bApplyGammaCorrection)
                {
                    Color = Color.ToGammaCorrectSpace(m_Pipeline->Gamma);
                }

                // Write color into color buffer
                m_Pipeline->ColorImage->SetPixelColor(PixelPosition, Color);
            }
        }
    }
}

static void GetTriangleBounds(const rndr::Point3r (&Positions)[3], rndr::Bounds2i& TriangleBounds)
{
    const rndr::Point2r Positions2D[] = {
        {Positions[0].X, Positions[0].Y},
        {Positions[1].X, Positions[1].Y},
        {Positions[2].X, Positions[2].Y},
    };

    const rndr::Point2r Min = rndr::Min(rndr::Min(Positions2D[0], Positions2D[1]), Positions2D[2]);
    const rndr::Point2r Max = rndr::Max(rndr::Max(Positions2D[0], Positions2D[1]), Positions2D[2]);

    // Min and Max are in continuous space, convert it to discrete space
    TriangleBounds.pMin = rndr::PixelCoordinates::ToDiscreteSpace(Min);
    TriangleBounds.pMax = rndr::PixelCoordinates::ToDiscreteSpace(Max);
}

static bool LimitTriangleToSurface(rndr::Bounds2i& TriangleBounds, const rndr::Image* Image)
{
    rndr::Bounds2i ImageBounds = Image->GetBounds();

    if (!rndr::Overlaps(TriangleBounds, ImageBounds))
    {
        return false;
    }

    TriangleBounds = rndr::Intersect(TriangleBounds, ImageBounds);

    return true;
}

static bool ShouldDiscardByDepth(const rndr::Point3r (&Points)[3])
{
    const real MinZ = std::min(std::min(Points[0].Z, Points[1].Z), Points[2].Z);
    const real MaxZ = std::max(std::max(Points[0].Z, Points[1].Z), Points[2].Z);

    return !((MinZ >= 0 && MinZ <= 1) || (MaxZ >= 0 && MaxZ <= 1) || (MinZ < 0 && MaxZ > 1));
}

bool rndr::Rasterizer::RunDepthTest(const Point2i& PixelPosition, real NewDepthValue)
{
    if (NewDepthValue < 0 || NewDepthValue > 1)
    {
        return false;
    }

    const real DestDepth = m_Pipeline->DepthImage->GetPixelDepth(PixelPosition);

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

rndr::Color rndr::Rasterizer::ApplyAlphaCompositing(const Point2i& PixelPosition, Color NewValue)
{
    rndr::Color CurrentColor = m_Pipeline->ColorImage->GetPixelColor(PixelPosition);
    if (m_Pipeline->bApplyGammaCorrection)
    {
        CurrentColor = CurrentColor.ToLinearSpace(m_Pipeline->Gamma);
    }
    real InvColorA = 1 - NewValue.A;

    NewValue.R = NewValue.A * NewValue.R + CurrentColor.R * CurrentColor.A * InvColorA;
    NewValue.G = NewValue.A * NewValue.G + CurrentColor.G * CurrentColor.A * InvColorA;
    NewValue.B = NewValue.A * NewValue.B + CurrentColor.B * CurrentColor.A * InvColorA;
    NewValue.A = NewValue.A + CurrentColor.A * InvColorA;

    return NewValue;
}

rndr::Point3r rndr::Rasterizer::FromNDCToRasterSpace(const Point3r& Point)
{
    int Width = m_Pipeline->ColorImage->GetConfig().Width;
    int Height = m_Pipeline->ColorImage->GetConfig().Height;

    Point3r Result = Point;
    Result.X = ((1 + Point.X) / 2) * Width;
    Result.Y = ((1 + Point.Y) / 2) * Height;

    return Result;
}

rndr::Point3r rndr::Rasterizer::FromRasterToNDCSpace(const Point3r& Point)
{
    int Width = m_Pipeline->ColorImage->GetConfig().Width;
    int Height = m_Pipeline->ColorImage->GetConfig().Height;

    Point3r Result = Point;
    Result.X = (Point.X / (real)Width) * 2 - 1;
    Result.Y = (Point.Y / (real)Height) * 2 - 1;

    return Result;
}
