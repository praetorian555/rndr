#pragma once

#include "rndr/core/base.h"
#include "rndr/core/math.h"
#include "rndr/core/span.h"

namespace rndr
{

class Mesh
{
public:
    // Takes ownership of the memory in Span objects!
    Mesh(const std::string& Name,
         Span<Point3r> Positions,
         Span<Vector2r> TexCoords,
         Span<Normal3r> Normals,
         Span<Vector3r> Tangents,
         Span<Vector3r> Bitangents,
         Span<int> Indices);
    ~Mesh();

    const std::string& GetName() const;
    const Span<Point3r>& GetPositions() const;
    const Span<Vector2r>& GetTexCoords() const;
    const Span<Normal3r>& GetNormals() const;
    const Span<Vector3r>& GetTangents() const;
    const Span<Vector3r>& GetBitangents() const;
    const Span<int>& GetIndices() const;

private:
    std::string m_Name;
    Span<Point3r> m_Positions;
    Span<Vector2r> m_TexCoords;
    Span<Normal3r> m_Normals;
    Span<Vector3r> m_Tangents;
    Span<Vector3r> m_Bitangents;
    Span<int> m_Indices;
};

}  // namespace rndr
