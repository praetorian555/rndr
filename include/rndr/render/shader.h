#pragma once

#include <functional>

#include "rndr/core/barycentric.h"
#include "rndr/core/color.h"
#include "rndr/core/math.h"

namespace rndr
{

struct PerPixelInfo
{
    Point2i Position;  // In discrete space
    BarycentricCoordinates BarCoords;

    // User specific data
    void* VertexData[3];
    void* InstanceData;  // Data unique for instance
    void* Constants;     // Data unique for shader

    PerPixelInfo* NextX;
    PerPixelInfo* NextY;

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
    FieldType Interpolate(size_t FieldOffset) const;

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
    ReturnType DerivativeX(size_t FieldOffset) const;

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
    ReturnType DerivativeY(size_t FieldOffset) const;
};

using PixelShaderCallback = std::function<Color(const PerPixelInfo&, real& DepthValue)>;

struct PixelShader
{
    bool bChangesDepth = false;
    PixelShaderCallback Callback;
};

struct PerVertexInfo
{
    int PrimitiveIndex;
    int VertexIndex;
    void* VertexData;    // Data specific for each vertex
    void* InstanceData;  // Data specific for each instance
    void* Constants;     // Data constant across all models and his instances
};

// Should return the point in NDC where x and y are in range [-1, 1] and z in range [0, 1]
using VertexShaderCallback = std::function<Point3r(const PerVertexInfo&)>;

struct VertexShader
{
    VertexShaderCallback Callback;
};

// Implementations ////////////////////////////////////////////////////////////////////////////////

template <typename FieldType, typename VertexDataType>
FieldType PerPixelInfo::Interpolate(size_t FieldOffset) const
{
    FieldType Return;
    for (int i = 0; i < 3; i++)
    {
        const uint8_t* Base = reinterpret_cast<const uint8_t*>(VertexData[i]);
        const FieldType* Field = reinterpret_cast<const FieldType*>(Base + FieldOffset);
        Return += BarCoords[i] * (*Field);
    }

    return Return;
}

template <typename FieldType, typename VertexDataType, typename ReturnType>
ReturnType PerPixelInfo::DerivativeX(size_t FieldOffset) const
{
    const FieldType Start = Interpolate<FieldType, VertexDataType>(FieldOffset);
    const FieldType End = NextX->Interpolate<FieldType, VertexDataType>(FieldOffset);

    return End - Start;
}

template <typename FieldType, typename VertexDataType, typename ReturnType>
ReturnType PerPixelInfo::DerivativeY(size_t FieldOffset) const
{
    const FieldType Start = Interpolate<FieldType, VertexDataType>(FieldOffset);
    const FieldType End = NextY->Interpolate<FieldType, VertexDataType>(FieldOffset);

    return End - Start;
}

}  // namespace rndr