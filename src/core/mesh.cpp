#include "rndr/core/mesh.h"

rndr::Mesh::Mesh(const std::string& Name,
                 Span<Point3r> Positions,
                 Span<Vector2r> TexCoords,
                 Span<Normal3r> Normals,
                 Span<Vector3r> Tangents,
                 Span<Vector3r> Bitangents,
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

const rndr::Span<rndr::Point3r>& rndr::Mesh::GetPositions() const
{
    return m_Positions;
}

const rndr::Span<rndr::Vector2r>& rndr::Mesh::GetTexCoords() const
{
    return m_TexCoords;
}

const rndr::Span<rndr::Normal3r>& rndr::Mesh::GetNormals() const
{
    return m_Normals;
}

const rndr::Span<rndr::Vector3r>& rndr::Mesh::GetTangents() const
{
    return m_Tangents;
}

const rndr::Span<rndr::Vector3r>& rndr::Mesh::GetBitangents() const
{
    return m_Bitangents;
}

const rndr::Span<int>& rndr::Mesh::GetIndices() const
{
    return m_Indices;
}
