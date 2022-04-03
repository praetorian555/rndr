#pragma once

#include <functional>

#include "rndr/core/barycentric.h"
#include "rndr/core/bounds2.h"
#include "rndr/core/color.h"
#include "rndr/core/math.h"

namespace rndr
{

struct Triangle;

/**
 * Holds vertex shader input data.
 */
struct InVertexInfo
{
    int VertexIndex;

    void* UserVertexData;    // Data specific for each vertex
    void* UserInstanceData;  // Data specific for each instance
    void* UserConstants;     // Data constant across all models and his instances
};

/**
 * Holds vertex shader output data.
 */
struct OutVertexInfo
{
    // Position of a vertex in the NDC space but not divided by W component.
    rndr::Point4r PositionNDCNonEucliean;

    // This should store data that should be interpolated for fragment shader. User defines the size
    // of this object as part of the model specification and rasterizer will allocate this memory so
    // that user only needs to cast it to his type.
    void* UserVertexData;
};

/**
 * Interface for a vertex shader implementation.
 */
using VertexShaderCallback = std::function<void(const InVertexInfo&, OutVertexInfo&)>;

/**
 * Holds vertex shader implementation and configuration.
 */
struct VertexShader
{
    VertexShaderCallback Callback;
};

/**
 * Holds the input data for a fragment shader.
 */
struct InFragmentInfo
{
    Point2i Position;  // In discrete space
    BarycentricCoordinates BarCoords;
    real W;
    real Depth;

    InFragmentInfo* NextX = nullptr;
    InFragmentInfo* NextY = nullptr;

    int NextXMult = 1;
    int NextYMult = 1;

    bool bIsInside = false;
};

/**
 * Holds the output data of a fragment shader.
 */
struct OutFragmentInfo
{
    rndr::Color Color;
    real Depth;
};

/**
 * Interface for a fragment shader implementation.
 */
using FragmentShaderCallback = std::function<void(const Triangle&, const InFragmentInfo&, OutFragmentInfo&)>;

/**
 * Holds fragment shader implementation and configuration.
 */
struct FragmentShader
{
    bool bChangesDepth = false;
    FragmentShaderCallback Callback;
};

struct Triangle
{
    rndr::Point3r ScreenPositions[3];
    OutVertexInfo* OutVertexData[3];
    void* ShaderConstants;

    real W[3];
    real OneOverW[3];
    real OneOverDepth[3];
    rndr::Bounds2i Bounds{{0, 0}, {0, 0}};
    rndr::BarycentricHelper BarHelper;

    InFragmentInfo* Fragments = nullptr;
    bool bIgnore = false;

#if RNDR_DEBUG
    int Indices[3];
    rndr::Bounds2i BoundsUnlimited;
    bool bOutsideXY;
    bool bOutsideZ;
    bool bBackFace;
#endif

    InFragmentInfo& GetFragmentInfo(int X, int Y)
    {
        assert(rndr::Inside(Point2i{X, Y}, Bounds));
        assert(Fragments);

        return Fragments[(X - Bounds.pMin.X) + (Y - Bounds.pMin.Y) * Bounds.Diagonal().X];
    }

    /**
     * Used to interpolate values in specified field using barycentric coordinates of this fragment.
     *
     * @tparam FieldType Name of the class used as a field in the vertex data.
     * @tparam VertexDataType Name of the class that stores vertex data.
     * @param Offset of field to interpolate in the VertexDataType class.
     *
     * @return Returns Interpolated value that is of the same type as the FieldType.
     */
    template <typename FieldType, typename VertexDataType>
    FieldType Interpolate(size_t FieldOffset, const InFragmentInfo& InInfo) const;

    /**
     * Calculates the rate of change of specified field compared to the next fragment along the X
     * axis.
     *
     * @tparam FieldType Name of the class used as a field in the vertex data.
     * @tparam VertexDataType Name of the class that stores vertex data.
     * @tparam ReturnType Used to specify different return type when subtraction of FieldType values
     * yields a new type.
     * @param Offset of field to interpolate in the VertexDataType class.
     *
     * @return Returns the rate of change of the specified field along the X axis.
     */
    template <typename FieldType, typename VertexDataType, typename ReturnType = FieldType>
    ReturnType DerivativeX(size_t FieldOffset, const InFragmentInfo& InInfo) const;

    /**
     * Calculates the rate of change of specified field compared to the next fragment along the Y
     * axis.
     *
     * @tparam FieldType Name of the class used as a field in the vertex data.
     * @tparam VertexDataType Name of the class that stores vertex data.
     * @tparam ReturnType Used to specify different return type when subtraction of FieldType values
     * yields a new type.
     * @param Offset of field to interpolate in the VertexDataType class.
     *
     * @return Returns the rate of change of the specified field along the Y axis.
     */
    template <typename FieldType, typename VertexDataType, typename ReturnType = FieldType>
    ReturnType DerivativeY(size_t FieldOffset, const InFragmentInfo& InInfo) const;
};

#define RNDR_INTERPOLATE(TriangleRef, VertexType, FieldType, FieldName, FragmentInfo) \
    TriangleRef.Interpolate<FieldType, VertexType>(offsetof(VertexType, FieldName), FragmentInfo);

#define RNDR_DX(TriangleRef, VertexType, FieldType, FieldName, ReturnType, FragmentInfo) \
    TriangleRef.DerivativeX<FieldType, VertexType, ReturnType>(offsetof(VertexType, FieldName), FragmentInfo);

#define RNDR_DY(TriangleRef, VertexType, FieldType, FieldName, ReturnType, FragmentInfo) \
    TriangleRef.DerivativeY<FieldType, VertexType, ReturnType>(offsetof(VertexType, FieldName), FragmentInfo);

// Implementations ////////////////////////////////////////////////////////////////////////////////

template <typename FieldType, typename VertexDataType>
FieldType Triangle::Interpolate(size_t FieldOffset, const InFragmentInfo& InInfo) const
{
    FieldType Return;
    for (int i = 0; i < 3; i++)
    {
        const uint8_t* Base = reinterpret_cast<const uint8_t*>(OutVertexData[i]->UserVertexData);
        const FieldType* Field = reinterpret_cast<const FieldType*>(Base + FieldOffset);
        Return += InInfo.BarCoords[i] * (*Field) * OneOverW[i];
    }

    Return *= InInfo.W;
    return Return;
}

template <typename FieldType, typename VertexDataType, typename ReturnType>
ReturnType Triangle::DerivativeX(size_t FieldOffset, const InFragmentInfo& InInfo) const
{
    const FieldType Start = Interpolate<FieldType, VertexDataType>(FieldOffset, InInfo);
    const FieldType End = InInfo.NextX ? Interpolate<FieldType, VertexDataType>(FieldOffset, *InInfo.NextX) : Start;

    return (End - Start) * (real)InInfo.NextXMult;
}

template <typename FieldType, typename VertexDataType, typename ReturnType>
ReturnType Triangle::DerivativeY(size_t FieldOffset, const InFragmentInfo& InInfo) const
{
    const FieldType Start = Interpolate<FieldType, VertexDataType>(FieldOffset, InInfo);
    const FieldType End = InInfo.NextY ? Interpolate<FieldType, VertexDataType>(FieldOffset, *InInfo.NextY) : Start;

    return (End - Start) * (real)InInfo.NextYMult;
}

}  // namespace rndr