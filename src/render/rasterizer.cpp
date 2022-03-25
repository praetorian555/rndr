#include "rndr/render/rasterizer.h"

#include "rndr/core/barycentric.h"
#include "rndr/core/bounds3.h"
#include "rndr/core/coordinates.h"
#include "rndr/core/log.h"
#include "rndr/core/threading.h"

#include "rndr/memory/stackallocator.h"

#include "rndr/render/image.h"
#include "rndr/render/model.h"

#include "rndr/profiling/cputracer.h"

// Helpers ////////////////////////////////////////////////////////////////////////////////////////

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
    TriangleBounds.pMax = rndr::PixelCoordinates::ToDiscreteSpace(Max + 1);
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

rndr::Rasterizer::Rasterizer()
{
    m_ScratchAllocator = new StackAllocator(RNDR_MB(1000));
}

void rndr::Rasterizer::SetPipeline(const rndr::Pipeline* Pipeline)
{
    m_Pipeline = Pipeline;
}

void rndr::Rasterizer::Draw(rndr::Model* Model, int InstanceCount)
{
    RNDR_CPU_TRACE("Draw Model");

    // Get number of triangles
    const std::vector<int>& Indices = Model->GetIndices();
    const int TriangleCountPerInstance = Indices.size() / 3;
    const int TriangleCount = TriangleCountPerInstance * InstanceCount;

    // Configure transient properties
    SetPipeline(Model->GetPipeline());
    m_Triangles.resize(TriangleCount);

#if RNDR_DEBUG
    m_TriangleDebugInfos.resize(TriangleCount);
#endif

    RunVertexShadersAndSetupTriangles(Model, InstanceCount);
    FindTrianglesToIgnore();
    AllocatePixelInfo();
    BarycentricCoordinates();
    SetupFragmentNeighbours();
    RunPixelShaders();

    m_ScratchAllocator->Reset();
}

void rndr::Rasterizer::RunVertexShadersAndSetupTriangles(Model* Model, int InstanceCount)
{
    RNDR_CPU_TRACE("Run Vertex Shaders");

    auto WorkOrder = [this, Model, InstanceCount](int TriangleIndex)
    {
        std::vector<uint8_t>& VertexData = Model->GetVertexData();
        const int VertexDataStride = Model->GetVertexDataStride();
        std::vector<uint8_t>& InstanceData = Model->GetInstanceData();
        const int InstanceDataStride = Model->GetInstanceDataStride();
        std::vector<int>& Indices = Model->GetIndices();

        const int TriangleCountPerInstance = Indices.size() / 3;
        assert(TriangleCountPerInstance);
        const int TriangleCount = TriangleCountPerInstance * InstanceCount;

        rndr::Triangle& T = m_Triangles[TriangleIndex];

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
            T.Positions[VertexIndex] =
                m_Pipeline->VertexShader->Callback(VertexInfo, T.W[VertexIndex]);
            assert(T.W[VertexIndex] != 0);
            T.OneOverW[VertexIndex] = 1 / T.W[VertexIndex];
            T.OneOverZ[VertexIndex] = 1 / T.Positions[VertexIndex].Z;

#if RNDR_DEBUG
            m_TriangleDebugInfos[TriangleIndex].NDCPosition[VertexIndex] = T.Positions[VertexIndex];
#endif

            T.Positions[VertexIndex] = FromNDCToRasterSpace(T.Positions[VertexIndex]);

#if RNDR_DEBUG
            m_TriangleDebugInfos[TriangleIndex].ScreenPosition[VertexIndex] =
                T.Positions[VertexIndex];
#endif
        };

        GetTriangleBounds(T.Positions, T.Bounds);

#if RNDR_DEBUG
        m_TriangleDebugInfos[TriangleIndex].OriginalBounds = T.Bounds;
#endif

        LimitTriangleToSurface(T.Bounds, m_Pipeline->ColorImage);

#if RNDR_DEBUG
        m_TriangleDebugInfos[TriangleIndex].ScreenBounds = T.Bounds;
#endif
    };

    const int TrianglesPerThread = 16;
    ParallelFor(m_Triangles.size(), TrianglesPerThread, WorkOrder);
}

void rndr::Rasterizer::FindTrianglesToIgnore()
{
    RNDR_CPU_TRACE("Discard Triangles");

    const Vector2i ZeroVector{0, 0};
    for (int TriangleIndex = 0; TriangleIndex < m_Triangles.size(); TriangleIndex++)
    {
        Triangle& T = m_Triangles[TriangleIndex];
        T.BarHelper = std::make_unique<BarycentricHelper>(m_Pipeline->WindingOrder, T.Positions);

        const bool bIsOutsideXY = T.Bounds.Diagonal() == ZeroVector;
        const bool bIsOutsideZ = ShouldDiscardByDepth(T.Positions);
        const bool bBackFace = !T.BarHelper->IsWindingOrderCorrect();

#if RNDR_DEBUG
        m_TriangleDebugInfos[TriangleIndex].bIsOutsideXY = bIsOutsideXY;
        m_TriangleDebugInfos[TriangleIndex].bIsOutsideZ = bIsOutsideZ;
        m_TriangleDebugInfos[TriangleIndex].bBackFace = bBackFace;
#endif

        T.bIgnore = bIsOutsideXY || bBackFace || bIsOutsideZ;
    }
}

void rndr::Rasterizer::AllocatePixelInfo()
{
    RNDR_CPU_TRACE("Allocate Fragment Memory");

    for (int TriangleIndex = 0; TriangleIndex < m_Triangles.size(); TriangleIndex++)
    {
        Triangle& T = m_Triangles[TriangleIndex];

        if (T.bIgnore)
        {
            continue;
        }

        const int PixelCount = T.Bounds.SurfaceArea();
        const int ByteCount = PixelCount * sizeof(PerPixelInfo);
        const int Alignment = 64;
        m_Triangles[TriangleIndex].Pixels =
            (PerPixelInfo*)m_ScratchAllocator->Allocate(ByteCount, Alignment);
    }
}

void rndr::Rasterizer::BarycentricCoordinates()
{
    RNDR_CPU_TRACE("Barycentric Coords");

    auto WorkOrder = [this](int TriangleIndex)
    {
        Triangle& T = m_Triangles[TriangleIndex];
        if (T.bIgnore)
        {
            return;
        }

        auto WorkOrder = [&](int X, int Y)
        {
            PerPixelInfo& PixelInfo = T.GetPixelInfo(X, Y);
            PixelInfo.VertexData[0] = T.Vertices[0].VertexData;
            PixelInfo.VertexData[1] = T.Vertices[1].VertexData;
            PixelInfo.VertexData[2] = T.Vertices[2].VertexData;
            PixelInfo.OneOverW[0] = T.OneOverW[0];
            PixelInfo.OneOverW[1] = T.OneOverW[1];
            PixelInfo.OneOverW[2] = T.OneOverW[2];
            PixelInfo.InstanceData = T.Vertices[0].InstanceData;
            PixelInfo.Constants = T.Vertices[0].Constants;
            PixelInfo.Position = Point2i{X, Y};
            PixelInfo.BarCoords = T.BarHelper->GetCoordinates(PixelInfo.Position);
            PixelInfo.bIsInside = T.BarHelper->IsInside(PixelInfo.BarCoords);
        };

        const int GridSideSize = 32;
        const Point2i EndPoint = T.Bounds.pMax;
        const Point2i StartPoint = T.Bounds.pMin;
        ParallelFor(EndPoint, GridSideSize, WorkOrder, StartPoint);
    };

    const int TrianglesPerThread = 1;
    ParallelFor(m_Triangles.size(), TrianglesPerThread, WorkOrder);
}

void rndr::Rasterizer::SetupFragmentNeighbours()
{
    RNDR_CPU_TRACE("Setup Fragment Neighbours");

    auto WorkOrder = [this](int TriangleIndex)
    {
        Triangle& T = m_Triangles[TriangleIndex];
        if (T.bIgnore)
        {
            return;
        }

        auto WorkOrder = [this, &T](int X, int Y)
        {
            PerPixelInfo& PixelInfo = T.GetPixelInfo(X, Y);
            if (!PixelInfo.bIsInside)
            {
                return;
            }

            // Find valid neighbour along X
            PixelInfo.NextX = nullptr;
            if (rndr::Inside({X + 1, Y}, T.Bounds) && T.GetPixelInfo(X + 1, Y).bIsInside)
            {
                PixelInfo.NextX = &T.GetPixelInfo(X + 1, Y);
                PixelInfo.NextXMult = 1;
            }
            else if (rndr::Inside({X - 1, Y}, T.Bounds) && T.GetPixelInfo(X - 1, Y).bIsInside)
            {
                PixelInfo.NextX = &T.GetPixelInfo(X - 1, Y);
                PixelInfo.NextXMult = -1;
            }

            // Find valid neighbour along Y
            PixelInfo.NextY = nullptr;
            if (rndr::Inside({X, Y + 1}, T.Bounds) && T.GetPixelInfo(X, Y + 1).bIsInside)
            {
                PixelInfo.NextY = &T.GetPixelInfo(X, Y + 1);
                PixelInfo.NextYMult = 1;
            }
            else if (rndr::Inside({X, Y - 1}, T.Bounds) && T.GetPixelInfo(X, Y - 1).bIsInside)
            {
                PixelInfo.NextY = &T.GetPixelInfo(X, Y - 1);
                PixelInfo.NextYMult = -1;
            }

            if (PixelInfo.NextX)
            {
                PixelInfo.NextX->W = 1 / PixelInfo.NextX->BarCoords.Interpolate(T.OneOverW);
            }

            if (PixelInfo.NextY)
            {
                PixelInfo.NextY->W = 1 / PixelInfo.NextY->BarCoords.Interpolate(T.OneOverW);
            }
        };

        const int GridSideSize = 32;
        const Point2i EndPoint = T.Bounds.pMax;
        const Point2i StartPoint = T.Bounds.pMin;
        ParallelFor(EndPoint, GridSideSize, WorkOrder, StartPoint);
    };

    const int TrianglesPerThread = 1;
    ParallelFor(m_Triangles.size(), TrianglesPerThread, WorkOrder);
}

void rndr::Rasterizer::RunPixelShaders()
{
    RNDR_CPU_TRACE("Run Pixel Shaders");

    const Point2i ImageSize = m_Pipeline->ColorImage->GetBounds().pMax + 1;
    const int BlockSize = 64;

    auto WorkOrder = [this, ImageSize, BlockSize](int BlockX, int BlockY)
    {
        const Point2i StartPoint{BlockX * BlockSize, BlockY * BlockSize};
        const Point2i Size{std::min(BlockSize, ImageSize.X - StartPoint.X),
                           std::min(BlockSize, ImageSize.Y - StartPoint.Y)};
        const Point2i EndPoint = StartPoint + Size;
        const Bounds2i BlockBounds{StartPoint, EndPoint};

        std::vector<Triangle*> OverlappingTriangles;
        for (Triangle& T : m_Triangles)
        {
            if (!T.bIgnore && rndr::Overlaps(T.Bounds, BlockBounds))
            {
                OverlappingTriangles.push_back(&T);
            }
        }

        for (Triangle* T : OverlappingTriangles)
        {
            for (int Y = StartPoint.Y; Y < EndPoint.Y; Y++)
            {
                for (int X = StartPoint.X; X < EndPoint.X; X++)
                {
                    if (!rndr::Inside({X, Y}, T->Bounds))
                    {
                        continue;
                    }

                    PerPixelInfo& PixelInfo = T->GetPixelInfo(X, Y);
                    if (!PixelInfo.bIsInside)
                    {
                        continue;
                    }

                    ProcessPixel(PixelInfo, *T);
                }
            }
        }
    };

    // Block will either have side size of BlockSize or whatever is left to the image's edge
    Point2i BlockGrid;
    BlockGrid.X =
        ImageSize.X % BlockSize == 0 ? ImageSize.X / BlockSize : (ImageSize.X / BlockSize) + 1;
    BlockGrid.Y =
        ImageSize.Y % BlockSize == 0 ? ImageSize.Y / BlockSize : (ImageSize.Y / BlockSize) + 1;

    const int BlocksPerThread = 1;
    ParallelFor(BlockGrid, BlocksPerThread, WorkOrder);
}

void rndr::Rasterizer::ProcessPixel(PerPixelInfo& PixelInfo, const Triangle& T)
{
    PixelInfo.W = 1 / PixelInfo.BarCoords.Interpolate(T.OneOverW);
    PixelInfo.Depth = 1 / PixelInfo.BarCoords.Interpolate(T.OneOverZ);
    assert(!rndr::IsNaN(PixelInfo.Depth));
    const real CurrentDepth = m_Pipeline->DepthImage->GetPixelDepth(PixelInfo.Position);

    // Early depth test
    if (!m_Pipeline->PixelShader->bChangesDepth)
    {
        if (!PerformDepthTest(m_Pipeline->DepthTest, PixelInfo.Depth, CurrentDepth))
        {
            return;
        }

        m_Pipeline->DepthImage->SetPixelDepth(PixelInfo.Position, PixelInfo.Depth);
    }

    // Run Pixel shader
    rndr::Color NewColor = m_Pipeline->PixelShader->Callback(PixelInfo, PixelInfo.Depth);

    // Standard depth test
    if (m_Pipeline->PixelShader->bChangesDepth)
    {
        if (!PerformDepthTest(m_Pipeline->DepthTest, PixelInfo.Depth, CurrentDepth))
        {
            return;
        }

        m_Pipeline->DepthImage->SetPixelDepth(PixelInfo.Position, PixelInfo.Depth);
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
