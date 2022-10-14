#include "rndr/render/model.h"

#include "rndr/render/material.h"
#include "rndr/render/mesh.h"

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
