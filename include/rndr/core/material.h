#pragma once

#include "math/vector3.h"

#include "rndr/core/base.h"

namespace rndr
{

// Forward declarations
struct Image;

class Material
{
public:
    Material() = default;

private:
    math::Vector3 m_Ka;  // Ambient color
    math::Vector3 m_Kd;  // Diffuse color
    math::Vector3 m_Ks;  // Specular color

    Image* m_AmbientImage = nullptr;
    Image* m_DiffuseImage = nullptr;
    Image* m_SpecularImage = nullptr;

    float m_Shininess;
};

}  // namespace rndr