#pragma once

#include <vector>

#include "rndr/core/base.h"
#include "rndr/core/span.h"

namespace rndr
{

// Forward declarations
class Mesh;
class Material;

class Model
{
public:
    Model() = default;

    void AddMesh(Mesh* Mesh);
    void AddMaterial(Material* Mat);

    Span<Mesh*> GetMeshes();

private:
    std::vector<Mesh*> m_Meshes;
    std::vector<Material*> m_Materials;
};

}  // namespace rndr