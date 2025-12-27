#pragma once

#include "opal/container/hash-map.h"
#include "opal/container/string.h"

#include "rndr/colors.hpp"
#include "rndr/enum-flags.hpp"
#include "rndr/math.hpp"
#include "rndr/platform/opengl-texture.hpp"

namespace Rndr
{

enum class MaterialFlags : u32
{
    NoFlags = 0,
    Transparent = 1 << 0,

};
RNDR_ENUM_CLASS_FLAGS(MaterialFlags);

struct MaterialDesc
{
    Vector4f albedo_color = Colors::k_pink;    // Used only if albedo texture is missing.
    Vector4f emissive_color = Colors::k_pink;  // Used only if emissive texture is missing.
    // Roughness of the surface, for anisotropic materials use the x and y while for isotropic materials the same value will be stored in
    // x and y. Used only if metallic-roughness texture is missing.
    Vector4f roughness = Vector4f{1.0f, 1.0f, 0.0f, 0.0f};
    f32 metalic = 0.0f;  // Used only if metallic-roughness texture is missing.

    f32 transparency_factor = 1.0f;
    f32 alpha = 0.0f;

    MaterialFlags material_flags = MaterialFlags::NoFlags;

    Opal::StringUtf8 albedo_texture_path;
    Opal::StringUtf8 emissive_texture_path;
    Opal::StringUtf8 metallic_roughness_texture_path;  // Metalic is stored in G channel, roughness in B channel.
    Opal::StringUtf8 normal_texture_path;
    Opal::StringUtf8 ambient_occlusion_texture_path;
    Opal::StringUtf8 opacity_texture_path;
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
    [[nodiscard]] bool HasEmissiveTexture() const { return (m_bit_mask & k_bit_mask_emissive_texture) != 0; }
    [[nodiscard]] const Vector4f& GetEmissiveColor() const { return m_desc.emissive_color; }
    [[nodiscard]] bool HasMetalicRoughnessTexture() const { return (m_bit_mask & k_bit_mask_metallic_roughness_texture) != 0; }
    [[nodiscard]] const Vector4f& GetRoughness() const { return m_desc.roughness; }
    [[nodiscard]] f32 GetMetalicFactor() const { return m_desc.metalic; }
    [[nodiscard]] f32 GetTransparencyFactor() const { return m_desc.transparency_factor; }
    [[nodiscard]] f32 GetAlphaTest() const { return m_desc.alpha; }
    [[nodiscard]] MaterialFlags GetMaterialFlags() const { return m_desc.material_flags; }
    [[nodiscard]] bool HasNormalTexture() const { return (m_bit_mask & k_bit_mask_normal_texture) != 0; }
    [[nodiscard]] bool HasAmbientOcclusionTexture() const { return (m_bit_mask & k_bit_mask_ambient_occlusion_texture) != 0; }
    [[nodiscard]] bool HasOpacityTexture() const { return (m_bit_mask & k_bit_mask_opacity_texture) != 0; }

    [[nodiscard]] Opal::Ref<const Texture> GetAlbedoTexture() const { return Opal::Ref{m_albedo_texture}; }
    [[nodiscard]] Opal::Ref<const Texture> GetEmissiveTexture() const { return Opal::Ref{m_emissive_texture}; }
    [[nodiscard]] Opal::Ref<const Texture> GetMetalicRoughnessTexture() const { return Opal::Ref{m_metallic_roughness_texture}; }
    [[nodiscard]] Opal::Ref<const Texture> GetNormalTexture() const { return Opal::Ref{m_normal_texture}; }
    [[nodiscard]] Opal::Ref<const Texture> GetAmbientOcclusionTexture() const { return Opal::Ref{m_ambient_occlusion_texture}; }
    [[nodiscard]] Opal::Ref<const Texture> GetOpacityTexture() const { return Opal::Ref{m_opacity_texture}; }

    [[nodiscard]] bool operator==(const Material& other) const;

private:
    friend struct Opal::Hasher<Material>;

    constexpr static u32 k_bit_mask_albedo_texture = 1 << 0;
    constexpr static u32 k_bit_mask_emissive_texture = 1 << 1;
    constexpr static u32 k_bit_mask_metallic_roughness_texture = 1 << 2;
    constexpr static u32 k_bit_mask_normal_texture = 1 << 3;
    constexpr static u32 k_bit_mask_ambient_occlusion_texture = 1 << 4;
    constexpr static u32 k_bit_mask_opacity_texture = 1 << 5;

    Texture LoadTexture(const Opal::StringUtf8& texture_path);

    MaterialDesc m_desc;
    u32 m_bit_mask = 0;

    Opal::Ref<GraphicsContext> m_graphics_context;
    Texture m_albedo_texture;
    Texture m_emissive_texture;
    Texture m_metallic_roughness_texture;
    Texture m_normal_texture;
    Texture m_ambient_occlusion_texture;
    Texture m_opacity_texture;
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
