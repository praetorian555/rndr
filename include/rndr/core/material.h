#pragma once

#include "rndr/core/base.h"
#include "rndr/core/math.h"

namespace rndr
{

// Forward declarations
struct Image;

class Material
{
public:
    Material() = default;

private:
    Vector3r m_Ka;  // Ambient color
    Vector3r m_Kd;  // Diffuse color
    Vector3r m_Ks;  // Specular color

    Image* m_AmbientImage = nullptr;
    Image* m_DiffuseImage = nullptr;
    Image* m_SpecularImage = nullptr;

    float m_Shininess;
};

}  // namespace rndr