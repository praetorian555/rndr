#pragma once

#include "rndr/core/base.h"

#ifdef RNDR_ASSIMP

#include <string_view>

#include "math/normal3.h"
#include "math/point2.h"
#include "math/point3.h"
#include "math/vector3.h"
#include "math/vector4.h"

#include "rndr/core/memory.h"
#include "rndr/utility/array.h"

namespace Assimp
{
class Importer;
}

namespace rndr
{

struct Model
{
    Array<math::Point3> Positions;
    Array<int> Indices;
    Array<math::Normal3> Normals;
    Array<math::Vector3> Tangents;
    Array<math::Vector3> Bitangents;
};

class ModelLoader
{
public:
    ModelLoader();
    ~ModelLoader();

    [[nodiscard]] ScopePtr<Model> Load(std::string ModelPath);

private:
    Assimp::Importer* m_Importer;
};

}  // namespace rndr

#endif  // RNDR_ASSIMP
