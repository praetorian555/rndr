#include "rndr/render/rasterizer.h"

#include "rndr/core/barycentric.h"
#include "rndr/core/bounds3.h"
#include "rndr/core/coordinates.h"
#include "rndr/core/log.h"
#include "rndr/core/model.h"
#include "rndr/core/threading.h"

#include "rndr/memory/stackallocator.h"

#include "rndr/profiling/cputracer.h"

#if defined RNDR_RASTER

#include "rndr/render/rasterimage.h"
#include "rndr/render/rasterpipeline.h"

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
        const rndr::Bounds2i ZeroBounds{rndr::Point2i(), rndr::Point2i()};
        TriangleBounds = ZeroBounds;
        return false;
    }

    TriangleBounds = rndr::Intersect(TriangleBounds, ImageBounds);

    return true;
}

static bool ShouldDiscardByDepth(const rndr::Point3r (&Points)[3])
{
    const real MinZ = std::min(std::min(Points[0].Z, Points[1].Z), Points[2].Z);
    const real MaxZ = std::max(std::max(Points[0].Z, Points[1].Z), Points[2].Z);

    // TODO(mkostic): Currently there is an issue when triangle is partially in the view box based on his depth. For this reason we will
    // discard any triangle that is partially outside the view box.
    return (MinZ < 0) || (MaxZ > 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

rndr::Rasterizer::Rasterizer()
{
    m_ScratchAllocator = new StackAllocator(RNDR_MB(1024));
}

void rndr::Rasterizer::SetPipeline(const rndr::Pipeline* Pipeline)
{
    m_Pipeline = Pipeline;
}

void rndr::Rasterizer::Draw(rndr::Model* Model)
{
    RNDR_CPU_TRACE("Draw Model");

    Setup(Model);

    RunVertexShaders();
    SetupTriangles();
    FindTrianglesToIgnore();
    AllocateFragmentInfo();
    BarycentricCoordinates();
    SetupFragmentNeighbours();
    RunFragmentShaders();

    m_ScratchAllocator->Reset();
}

void rndr::Rasterizer::Setup(Model* Model)
{
    m_Model = Model;
    m_Pipeline = Model->GetPipeline();
    m_InstanceCount = Model->GetInstanceCount();
    m_VerticesPerInstanceCount = Model->GetVertexCount();
    m_TrianglePerInstanceCount = Model->GetIndices().Size / 3;
    const int VerticesCount = m_InstanceCount * m_VerticesPerInstanceCount;
    const int TriangleCount = m_InstanceCount * m_TrianglePerInstanceCount;
    m_Triangles.Size = TriangleCount;
    m_Triangles.Data = (Triangle*)m_ScratchAllocator->Allocate(TriangleCount * sizeof(Triangle), 64);
    m_Vertices.Size = VerticesCount;
    m_Vertices.Data = (OutVertexInfo*)m_ScratchAllocator->Allocate(VerticesCount * sizeof(OutVertexInfo), 64);
    m_UserVertices.Size = VerticesCount * Model->GetOutVertexStride();
    m_UserVertices.Data = (uint8_t*)m_ScratchAllocator->Allocate(m_UserVertices.Size, 64);
}

void rndr::Rasterizer::RunVertexShaders()
{
    RNDR_CPU_TRACE("Vertex Shaders");

    void* UserConstants = m_Model->GetShaderConstants().Data;
    uint8_t* UserVertexData = m_Model->GetVertexData().Data;
    const int VertexDataStride = m_Model->GetVertexStride();
    uint8_t* UserInstanceData = m_Model->GetInstanceData().Data;
    const int InstanceDataStride = m_Model->GetInstanceStride();
    const int OutVertexStride = m_Model->GetOutVertexStride();

    auto WorkOrder = [=](int VertexIndex)
    {
        const int ModelVertexIndex = VertexIndex % m_VerticesPerInstanceCount;
        const int InstanceIndex = VertexIndex / m_VerticesPerInstanceCount;

        InVertexInfo InInfo;
        InInfo.VertexIndex = VertexIndex;
        InInfo.UserVertexData = UserVertexData + ModelVertexIndex * VertexDataStride;
        InInfo.UserInstanceData = UserInstanceData + InstanceIndex * InstanceDataStride;
        InInfo.UserConstants = UserConstants;
        OutVertexInfo& OutInfo = *(m_Vertices.Data + VertexIndex);
        OutInfo.UserVertexData = m_UserVertices.Data + VertexIndex * OutVertexStride;
        m_Pipeline->VertexShader(InInfo, OutInfo);
    };

    const int VerticesPerThread = 64;
    ParallelFor(m_Vertices.Size, VerticesPerThread, WorkOrder);
}

void rndr::Rasterizer::SetupTriangles()
{
    RNDR_CPU_TRACE("Setup Triangles");

    IntSpan Indices = m_Model->GetIndices();
    void* ShaderConstants = m_Model->GetShaderConstants().Data;

    auto WorkOrder = [=](int TriangleIndex)
    {
        const int ModelTriangleIndex = TriangleIndex % m_TrianglePerInstanceCount;
        const int InstanceIndex = TriangleIndex / m_TrianglePerInstanceCount;
        const int BaseVertexPosition = InstanceIndex * m_VerticesPerInstanceCount;
        const int BaseIndex = ModelTriangleIndex * 3;
        const int VertexIndex0 = BaseVertexPosition + Indices.Data[BaseIndex];
        const int VertexIndex1 = BaseVertexPosition + Indices.Data[BaseIndex + 1];
        const int VertexIndex2 = BaseVertexPosition + Indices.Data[BaseIndex + 2];

        rndr::Point3r NDCPositions[] = {(rndr::Point3r)m_Vertices[VertexIndex0].PositionNDCNonEucliean.ToEuclidean(),
                                        (rndr::Point3r)m_Vertices[VertexIndex1].PositionNDCNonEucliean.ToEuclidean(),
                                        (rndr::Point3r)m_Vertices[VertexIndex2].PositionNDCNonEucliean.ToEuclidean()};

        Triangle& T = m_Triangles[TriangleIndex];
        T.W[0] = m_Vertices[VertexIndex0].PositionNDCNonEucliean.W;
        T.W[1] = m_Vertices[VertexIndex1].PositionNDCNonEucliean.W;
        T.W[2] = m_Vertices[VertexIndex2].PositionNDCNonEucliean.W;
        T.OneOverW[0] = 1 / T.W[0];
        T.OneOverW[1] = 1 / T.W[1];
        T.OneOverW[2] = 1 / T.W[2];
        T.ScreenPositions[0] = FromNDCToRasterSpace(NDCPositions[0]);
        T.ScreenPositions[1] = FromNDCToRasterSpace(NDCPositions[1]);
        T.ScreenPositions[2] = FromNDCToRasterSpace(NDCPositions[2]);
        T.OneOverDepth[0] = 1 / T.ScreenPositions[0].Z;
        T.OneOverDepth[1] = 1 / T.ScreenPositions[1].Z;
        T.OneOverDepth[2] = 1 / T.ScreenPositions[2].Z;
        T.OutVertexData[0] = &m_Vertices[VertexIndex0];
        T.OutVertexData[1] = &m_Vertices[VertexIndex1];
        T.OutVertexData[2] = &m_Vertices[VertexIndex2];
        T.ShaderConstants = ShaderConstants;
        Bounds2i Bounds;
        GetTriangleBounds(T.ScreenPositions, Bounds);
        T.Bounds = Bounds;
        LimitTriangleToSurface(T.Bounds, m_Pipeline->ColorImage);
        T.BarHelper = BarycentricHelper(m_Pipeline->WindingOrder, T.ScreenPositions);

#if RNDR_DEBUG
        T.Indices[0] = VertexIndex0;
        T.Indices[1] = VertexIndex1;
        T.Indices[2] = VertexIndex2;
        T.BoundsUnlimited = Bounds;
#endif
    };

    const int TrianglesPerThread = 64;
    ParallelFor(m_Triangles.Size, TrianglesPerThread, WorkOrder);
}

void rndr::Rasterizer::FindTrianglesToIgnore()
{
    RNDR_CPU_TRACE("Discard Triangles");

    const Vector2i ZeroVector{0, 0};
    auto WorkOrder = [=](int TriangleIndex)
    {
        {
            Triangle& T = m_Triangles[TriangleIndex];

            const bool bIsOutsideXY = T.Bounds.Diagonal() == ZeroVector;
            const bool bIsOutsideZ = ShouldDiscardByDepth(T.ScreenPositions);
            const bool bBackFace = !T.BarHelper.IsWindingOrderCorrect();

#if RNDR_DEBUG
            T.bOutsideXY = bIsOutsideXY;
            T.bOutsideZ = bIsOutsideZ;
            T.bBackFace = bBackFace;
#endif

            T.bIgnore = bIsOutsideXY || bBackFace || bIsOutsideZ;
        }
    };

    const int TrianglesPerThread = 64;
    ParallelFor(m_Triangles.Size, TrianglesPerThread, WorkOrder);
}

void rndr::Rasterizer::AllocateFragmentInfo()
{
    RNDR_CPU_TRACE("Allocate Fragment Memory");

    for (int TriangleIndex = 0; TriangleIndex < m_Triangles.Size; TriangleIndex++)
    {
        Triangle& T = m_Triangles[TriangleIndex];

        if (T.bIgnore)
        {
            continue;
        }

        const int PixelCount = T.Bounds.SurfaceArea();
        const int ByteCount = PixelCount * sizeof(InFragmentInfo);
        const int Alignment = 64;
        m_Triangles[TriangleIndex].Fragments = (InFragmentInfo*)m_ScratchAllocator->Allocate(ByteCount, Alignment);
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
            InFragmentInfo& InInfo = T.GetFragmentInfo(X, Y);
            InInfo.Position = Point2i{X, Y};
            InInfo.BarCoords = T.BarHelper.GetCoordinates(InInfo.Position);
            InInfo.bIsInside = T.BarHelper.IsInside(InInfo.BarCoords);
        };

        const int GridSideSize = 32;
        const Point2i EndPoint = T.Bounds.pMax;
        const Point2i StartPoint = T.Bounds.pMin;
        ParallelFor(EndPoint, GridSideSize, WorkOrder, StartPoint);
    };

    const int TrianglesPerThread = 64;
    ParallelFor(m_Triangles.Size, TrianglesPerThread, WorkOrder);
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
            InFragmentInfo& InInfo = T.GetFragmentInfo(X, Y);
            if (!InInfo.bIsInside)
            {
                return;
            }

            // Find valid neighbour along X
            InInfo.NextX = nullptr;
            if (rndr::Inside({X + 1, Y}, T.Bounds) && T.GetFragmentInfo(X + 1, Y).bIsInside)
            {
                InInfo.NextX = &T.GetFragmentInfo(X + 1, Y);
                InInfo.NextXMult = 1;
            }
            else if (rndr::Inside({X - 1, Y}, T.Bounds) && T.GetFragmentInfo(X - 1, Y).bIsInside)
            {
                InInfo.NextX = &T.GetFragmentInfo(X - 1, Y);
                InInfo.NextXMult = -1;
            }

            // Find valid neighbour along Y
            InInfo.NextY = nullptr;
            if (rndr::Inside({X, Y + 1}, T.Bounds) && T.GetFragmentInfo(X, Y + 1).bIsInside)
            {
                InInfo.NextY = &T.GetFragmentInfo(X, Y + 1);
                InInfo.NextYMult = 1;
            }
            else if (rndr::Inside({X, Y - 1}, T.Bounds) && T.GetFragmentInfo(X, Y - 1).bIsInside)
            {
                InInfo.NextY = &T.GetFragmentInfo(X, Y - 1);
                InInfo.NextYMult = -1;
            }

            if (InInfo.NextX)
            {
                InInfo.NextX->W = 1 / InInfo.NextX->BarCoords.Interpolate(T.OneOverW);
            }

            if (InInfo.NextY)
            {
                InInfo.NextY->W = 1 / InInfo.NextY->BarCoords.Interpolate(T.OneOverW);
            }
        };

        const int GridSideSize = 32;
        const Point2i EndPoint = T.Bounds.pMax;
        const Point2i StartPoint = T.Bounds.pMin;
        ParallelFor(EndPoint, GridSideSize, WorkOrder, StartPoint);
    };

    const int TrianglesPerThread = 64;
    ParallelFor(m_Triangles.Size, TrianglesPerThread, WorkOrder);
}

void rndr::Rasterizer::RunFragmentShaders()
{
    RNDR_CPU_TRACE("Run Fragment Shaders");

    const Point2i ImageSize = m_Pipeline->ColorImage->GetBounds().pMax + 1;
    const int BlockSize = 32;

    auto WorkOrder = [this, ImageSize, BlockSize](int BlockX, int BlockY)
    {
        const Point2i StartPoint{BlockX * BlockSize, BlockY * BlockSize};
        const Point2i Size{std::min(BlockSize, ImageSize.X - StartPoint.X), std::min(BlockSize, ImageSize.Y - StartPoint.Y)};
        const Point2i EndPoint = StartPoint + Size;
        const Bounds2i BlockBounds{StartPoint, EndPoint};

        for (int TriangleIndex = 0; TriangleIndex < m_Triangles.Size; TriangleIndex++)
        {
            Triangle& T = m_Triangles[TriangleIndex];
            if (T.bIgnore || !rndr::Overlaps(T.Bounds, BlockBounds))
            {
                continue;
            }

            for (int Y = StartPoint.Y; Y < EndPoint.Y; Y++)
            {
                for (int X = StartPoint.X; X < EndPoint.X; X++)
                {
                    if (!rndr::Inside({X, Y}, T.Bounds))
                    {
                        continue;
                    }

                    InFragmentInfo& InInfo = T.GetFragmentInfo(X, Y);
                    if (!InInfo.bIsInside)
                    {
                        continue;
                    }

                    ProcessFragment(T, InInfo);
                }
            }
        }
    };

    // Block will either have side size of BlockSize or whatever is left to the image's edge
    Point2i BlockGrid;
    BlockGrid.X = ImageSize.X % BlockSize == 0 ? ImageSize.X / BlockSize : (ImageSize.X / BlockSize) + 1;
    BlockGrid.Y = ImageSize.Y % BlockSize == 0 ? ImageSize.Y / BlockSize : (ImageSize.Y / BlockSize) + 1;

    const int BlocksPerThread = 1;
    ParallelFor(BlockGrid, BlocksPerThread, WorkOrder);
}

void rndr::Rasterizer::ProcessFragment(const Triangle& T, InFragmentInfo& InInfo)
{
    InInfo.W = 1 / InInfo.BarCoords.Interpolate(T.OneOverW);
    InInfo.Depth = 1 / InInfo.BarCoords.Interpolate(T.OneOverDepth);
    assert(!rndr::IsNaN(InInfo.Depth));
    const real CurrentDepth = m_Pipeline->DepthImage->GetPixelDepth(InInfo.Position);

    // Early depth test
    if (!m_Pipeline->bChangesDepth)
    {
        if (!m_Pipeline->DepthTest(InInfo.Depth, CurrentDepth))
        {
            return;
        }

        m_Pipeline->DepthImage->SetPixelDepth(InInfo.Position, InInfo.Depth);
    }

    // Run Pixel shader
    OutFragmentInfo OutInfo;
    OutInfo.Depth = InInfo.Depth;
    m_Pipeline->FragmentShader(T, InInfo, OutInfo);

    // Standard depth test
    if (m_Pipeline->bChangesDepth)
    {
        if (!m_Pipeline->DepthTest(OutInfo.Depth, CurrentDepth))
        {
            return;
        }

        m_Pipeline->DepthImage->SetPixelDepth(InInfo.Position, InInfo.Depth);
    }

    // TODO(mkostic): Add support for different blend operators
    Color CurrentColor = m_Pipeline->ColorImage->GetPixelColor(InInfo.Position);
    OutInfo.Color = Color::Blend(OutInfo.Color, CurrentColor);

    // Write color into color buffer
    m_Pipeline->ColorImage->SetPixelColor(InInfo.Position, OutInfo.Color);
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

#endif  // RNDR_RASTER
