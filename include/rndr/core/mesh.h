#pragma once

#include "math/normal3.h"
#include "math/point2.h"
#include "math/vector2.h"
#include "math/vector3.h"

#include "rndr/core/base.h"
#include "rndr/core/span.h"

namespace rndr
{

class Mesh
{
public:
    // Takes ownership of the memory in Span objects!
    Mesh(const std::string& Name,
         Span<math::Point3> Positions,
         Span<math::Vector2> TexCoords,
         Span<math::Normal3> Normals,
         Span<math::Vector3> Tangents,
         Span<math::Vector3> Bitangents,
         Span<int> Indices);
    ~Mesh();

    const std::string& GetName() const;
    const Span<math::Point3>& GetPositions() const;
    const Span<math::Vector2>& GetTexCoords() const;
    const Span<math::Normal3>& GetNormals() const;
    const Span<math::Vector3>& GetTangents() const;
    const Span<math::Vector3>& GetBitangents() const;
    const Span<int>& GetIndices() const;

private:
    std::string m_Name;
    Span<math::Point3> m_Positions;
    Span<math::Vector2> m_TexCoords;
    Span<math::Normal3> m_Normals;
    Span<math::Vector3> m_Tangents;
    Span<math::Vector3> m_Bitangents;
    Span<int> m_Indices;
};

}  // namespace rndr
