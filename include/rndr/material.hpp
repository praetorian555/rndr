#pragma once

#include "opal/container/string.h"
#include "opal/container/hash-map.h"

#include "rndr/platform/opengl-texture.hpp"
#include "rndr/colors.hpp"
#include "rndr/math.hpp"

namespace Rndr
{

struct MaterialDesc
{
    Vector4f albedo_color = Colors::k_pink;
    Opal::StringUtf8 albedo_texture_path;
};

/**
 * Encapsulates parameters and textures needed for physically-based rendering of meshes.
 */
class Material
{
public:
    explicit Material(Opal::Ref<GraphicsContext> graphics_context, const MaterialDesc& desc);

    [[nodiscard]] bool HasAlbedoTexture() const { return (m_bit_mask & k_bit_mask_albedo_texture) != 0; }
    [[nodiscard]] const Vector4f& GetAlbedoColor() const { return m_desc.albedo_color; }
    [[nodiscard]] Opal::Ref<const Texture> GetAlbedoTexture() const { return Opal::Ref{m_albedo_texture}; }

    [[nodiscard]] bool operator==(const Material& other) const;

private:
    friend struct Opal::Hasher<Material>;

    constexpr static u32 k_bit_mask_albedo_texture = 1;

    MaterialDesc m_desc;
    u32 m_bit_mask = 0;

    Opal::Ref<GraphicsContext> m_graphics_context;
    Texture m_albedo_texture;
};

class MaterialRegistry
{
public:
    explicit MaterialRegistry(Opal::Ref<GraphicsContext> graphics_context) : m_graphics_context(graphics_context) {}
    void Register(Opal::StringUtf8 name, const MaterialDesc& desc);
    Opal::Ref<const Material> Get(const Opal::StringUtf8& name) const;

private:
    Opal::Ref<GraphicsContext> m_graphics_context;
    Opal::HashMap<Opal::StringUtf8, Material> m_materials;
};

}  // namespace Rndr
