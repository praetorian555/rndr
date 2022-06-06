#include "rndr/core/model.h"

#include "rndr/core/material.h"
#include "rndr/core/mesh.h"

void rndr::Model::AddMesh(Mesh* Mesh)
{
    assert(Mesh);
    m_Meshes.push_back(Mesh);
}

void rndr::Model::AddMaterial(Material* Mat)
{
    assert(Mat);
    m_Materials.push_back(Mat);
}

rndr::Span<rndr::Mesh*> rndr::Model::GetMeshes()
{
    return Span<Mesh*>(m_Meshes);
}
