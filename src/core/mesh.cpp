#include "rndr/core/mesh.h"

rndr::Mesh::Mesh(const std::string& Name,
                 Span<math::Point3> Positions,
                 Span<math::Vector2> TexCoords,
                 Span<math::Normal3> Normals,
                 Span<math::Vector3> Tangents,
                 Span<math::Vector3> Bitangents,
                 Span<int> Indices)
    : m_Name(Name),
      m_Positions(Positions),
      m_TexCoords(TexCoords),
      m_Normals(Normals),
      m_Tangents(Tangents),
      m_Bitangents(Bitangents),
      m_Indices(Indices)
{
}

rndr::Mesh::~Mesh()
{
    delete[] m_Positions.Data;
    delete[] m_TexCoords.Data;
    delete[] m_Normals.Data;
    delete[] m_Tangents.Data;
    delete[] m_Bitangents.Data;
    delete[] m_Indices.Data;
}

const std::string& rndr::Mesh::GetName() const
{
    return m_Name;
}

const rndr::Span<math::Point3>& rndr::Mesh::GetPositions() const
{
    return m_Positions;
}

const rndr::Span<math::Vector2>& rndr::Mesh::GetTexCoords() const
{
    return m_TexCoords;
}

const rndr::Span<math::Normal3>& rndr::Mesh::GetNormals() const
{
    return m_Normals;
}

const rndr::Span<math::Vector3>& rndr::Mesh::GetTangents() const
{
    return m_Tangents;
}

const rndr::Span<math::Vector3>& rndr::Mesh::GetBitangents() const
{
    return m_Bitangents;
}

const rndr::Span<int>& rndr::Mesh::GetIndices() const
{
    return m_Indices;
}
