#include "rndr/render/rasterizer.h"

#include "rndr/core/barycentric.h"
#include "rndr/core/bounds3.h"
#include "rndr/core/coordinates.h"
#include "rndr/core/log.h"
#include "rndr/core/threading.h"

#include "rndr/render/image.h"
#include "rndr/render/model.h"

#include "rndr/profiling/cputracer.h"

// Helpers ////////////////////////////////////////////////////////////////////////////////////////

struct rndr::Triangle
{
    rndr::PerVertexInfo Vertices[3];
    rndr::Point3r Positions[3];
    rndr::Bounds2i Bounds{{0, 0}, {0, 0}};
    std::unique_ptr<rndr::BarycentricHelper> BarHelper;
};

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

///////////////////////////////////////////////////////////////////////////////////////////////////

void rndr::Rasterizer::SetPipeline(const rndr::Pipeline* Pipeline)
{
    m_Pipeline = Pipeline;
}

void rndr::Rasterizer::Draw(rndr::Model* Model, int InstanceCount)
{
    RNDR_CPU_TRACE("Draw Model");

    m_Pipeline = Model->GetPipeline();

    const std::vector<int>& Indices = Model->GetIndices();

    const int TriangleCountPerInstance = Indices.size() / 3;
    const int TriangleCount = TriangleCountPerInstance * InstanceCount;
    std::vector<Triangle> Triangles(TriangleCount);

    // Run vertex shaders for all triangles
    auto VertexShaderProcessor = [&](int TriangleIndex)
    {
        std::vector<uint8_t>& VertexData = Model->GetVertexData();
        const int VertexDataStride = Model->GetVertexDataStride();
        std::vector<uint8_t>& InstanceData = Model->GetInstanceData();
        const int InstanceDataStride = Model->GetInstanceDataStride();
        std::vector<int>& Indices = Model->GetIndices();

        const int TriangleCountPerInstance = Indices.size() / 3;
        const int TriangleCount = TriangleCountPerInstance * InstanceCount;

        rndr::Triangle& T = Triangles[TriangleIndex];

        const int InstanceIndex = TriangleIndex / TriangleCountPerInstance;
        const int Offset = InstanceIndex * InstanceDataStride;
        void* InstancePtr = InstanceData.empty() ? nullptr : (void*)&InstanceData[Offset];

        for (int VertexIndex = 0; VertexIndex < 3; VertexIndex++)
        {
            const int IndexOfIndex = (TriangleIndex % TriangleCountPerInstance) * 3 + VertexIndex;
            rndr::PerVertexInfo& VertexInfo = T.Vertices[VertexIndex];
            VertexInfo.PrimitiveIndex = TriangleIndex;
            VertexInfo.VertexIndex = Indices[IndexOfIndex];
            VertexInfo.VertexData =
                (void*)&(VertexData.data()[VertexInfo.VertexIndex * VertexDataStride]);
            VertexInfo.InstanceData = InstancePtr;
            VertexInfo.Constants = Model->GetConstants();
            assert(VertexInfo.VertexData);

            // Run Vertex shader
            T.Positions[VertexIndex] = m_Pipeline->VertexShader->Callback(VertexInfo);
            T.Positions[VertexIndex] = FromNDCToRasterSpace(T.Positions[VertexIndex]);
        };

        GetTriangleBounds(T.Positions, T.Bounds);
        LimitTriangleToSurface(T.Bounds, m_Pipeline->ColorImage);
    };

    ParallelFor(TriangleCount, 16, VertexShaderProcessor);

    // Check if we can discard any of the triangles
    const Vector2i ZeroVector{0, 0};
    for (int TriangleIndex = 0; TriangleIndex < Triangles.size(); TriangleIndex++)
    {
        Triangle& T = Triangles[TriangleIndex];
        T.BarHelper = std::make_unique<BarycentricHelper>(m_Pipeline->WindingOrder, T.Positions);

        const bool bIsOutsideXY = T.Bounds.Diagonal() == ZeroVector;
        const bool bIsOutsideZ = ShouldDiscardByDepth(T.Positions);
        const bool bBackFace = !T.BarHelper->IsWindingOrderCorrect();

        if (bIsOutsideXY || bBackFace || bIsOutsideZ)
        {
            std::swap(T, Triangles.back());
            Triangles.pop_back();
            TriangleIndex--;
        }
    }

    const Point2i ImageSize = m_Pipeline->ColorImage->GetBounds().pMax + 1;
    const int BlockSize = 16;
    Point2i BlockGrid;
    BlockGrid.X =
        ImageSize.X % BlockSize == 0 ? ImageSize.X / BlockSize : (ImageSize.X / BlockSize) + 1;
    BlockGrid.Y =
        ImageSize.Y % BlockSize == 0 ? ImageSize.Y / BlockSize : (ImageSize.Y / BlockSize) + 1;

    auto& PixelShaderProcessor = [&](int BlockX, int BlockY)
    {
        const Point2i StartPoint{BlockX * BlockSize, BlockY * BlockSize};
        const Point2i Size{std::min(BlockSize, ImageSize.X - StartPoint.X),
                           std::min(BlockSize, ImageSize.Y - StartPoint.Y)};
        const Point2i EndPoint = StartPoint + Size;
        const Bounds2i BlockBounds{StartPoint, EndPoint};

        std::vector<const Triangle*> OverlappingTriangles;
        for (const Triangle& T : Triangles)
        {
            if (rndr::Overlaps(T.Bounds, BlockBounds))
            {
                OverlappingTriangles.push_back(&T);
            }
        }

        for (const Triangle* T : OverlappingTriangles)
        {
            PerPixelInfo PixelInfo;
            PixelInfo.VertexData[0] = T->Vertices[0].VertexData;
            PixelInfo.VertexData[1] = T->Vertices[1].VertexData;
            PixelInfo.VertexData[2] = T->Vertices[2].VertexData;
            PixelInfo.InstanceData = T->Vertices[0].InstanceData;
            PixelInfo.Constants = T->Vertices[0].Constants;
            assert(PixelInfo.VertexData[0]);
            assert(PixelInfo.VertexData[1]);
            assert(PixelInfo.VertexData[2]);

            for (int Y = StartPoint.Y; Y < EndPoint.Y; Y++)
            {
                for (int X = StartPoint.X; X < EndPoint.X; X++)
                {
                    PixelInfo.Position = Point2i{X, Y};
                    PixelInfo.BarCoords = T->BarHelper->GetCoordinates(PixelInfo.Position);

                    ProcessPixel(PixelInfo, *T);
                }
            }
        }
    };

    ParallelFor(BlockGrid, 1, PixelShaderProcessor);
}

void rndr::Rasterizer::ProcessPixel(const PerPixelInfo& PixelInfo, const Triangle& T)
{
    if (!T.BarHelper->IsInside(PixelInfo.BarCoords))
    {
        return;
    }

    real NewDepth = 1 / PixelInfo.BarCoords.Interpolate(T.BarHelper->m_OneOverPointDepth);
    const real CurrentDepth = m_Pipeline->DepthImage->GetPixelDepth(PixelInfo.Position);

    // Early depth test
    if (!m_Pipeline->PixelShader->bChangesDepth)
    {
        if (!PerformDepthTest(m_Pipeline->DepthTest, NewDepth, CurrentDepth))
        {
            return;
        }

        m_Pipeline->DepthImage->SetPixelDepth(PixelInfo.Position, NewDepth);
    }

    // Run Pixel shader
    rndr::Color NewColor = m_Pipeline->PixelShader->Callback(PixelInfo, NewDepth);

    // Standard depth test
    if (m_Pipeline->PixelShader->bChangesDepth)
    {
        if (!PerformDepthTest(m_Pipeline->DepthTest, NewDepth, CurrentDepth))
        {
            return;
        }

        m_Pipeline->DepthImage->SetPixelDepth(PixelInfo.Position, NewDepth);
    }

    // TODO(mkostic): Add support for different blend operators
    Color CurrentColor = m_Pipeline->ColorImage->GetPixelColor(PixelInfo.Position);
    NewColor = Color::Blend(NewColor, CurrentColor);

    // Write color into color buffer
    m_Pipeline->ColorImage->SetPixelColor(PixelInfo.Position, NewColor);
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

bool rndr::Rasterizer::PerformDepthTest(rndr::DepthTest Operator, real Src, real Dst)
{
    if (Src < 0 || Src > 1)
    {
        return false;
    }

    switch (Operator)
    {
        case rndr::DepthTest::GreaterThan:
        {
            return Src > Dst;
        }
        case rndr::DepthTest::LesserThen:
        {
            return Src < Dst;
        }
        case rndr::DepthTest::None:
        {
            return true;
        }
        default:
        {
            RNDR_LOG_ERROR("Rasterizer::PerformDepthTest: Unsupported operator supplied! Got %u",
                           (uint32_t)Operator);
            assert(false);
        }
    }

    return true;
}
