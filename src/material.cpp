#include "rndr/material.hpp"

#include "opal/file-system.h"

#include "rndr/file.hpp"
#include "rndr/graphics-types.hpp"

#define SET_MATERIAL_BIT(texture, bit) \
    if (texture.IsValid())             \
    {                                  \
        m_bit_mask |= bit;             \
    }                                  \
    else                               \
    {                                  \
        m_bit_mask &= ~bit;            \
    }

Rndr::Material::Material(Opal::Ref<GraphicsContext> graphics_context, const MaterialDesc& desc)
    : m_desc(desc), m_graphics_context(graphics_context)
{
    m_albedo_texture = LoadTexture(desc.albedo_texture_path);
    SET_MATERIAL_BIT(m_albedo_texture, k_bit_mask_albedo_texture);
    m_emissive_texture = LoadTexture(desc.emissive_texture_path);
    SET_MATERIAL_BIT(m_emissive_texture, k_bit_mask_emissive_texture);
    m_metallic_roughness_texture = LoadTexture(desc.metallic_roughness_texture_path);
    SET_MATERIAL_BIT(m_metallic_roughness_texture, k_bit_mask_metallic_roughness_texture);
    m_normal_texture = LoadTexture(desc.normal_texture_path);
    SET_MATERIAL_BIT(m_normal_texture, k_bit_mask_normal_texture);
    m_ambient_occlusion_texture = LoadTexture(desc.ambient_occlusion_texture_path);
    SET_MATERIAL_BIT(m_ambient_occlusion_texture, k_bit_mask_ambient_occlusion_texture);
    m_opacity_texture = LoadTexture(desc.opacity_texture_path);
    SET_MATERIAL_BIT(m_opacity_texture, k_bit_mask_opacity_texture);
}

Rndr::Material::Material(Material&& other) noexcept
    : m_desc(std::move(other.m_desc)),
      m_bit_mask(other.m_bit_mask),
      m_graphics_context(std::move(other.m_graphics_context)),
      m_albedo_texture(std::move(other.m_albedo_texture)),
      m_emissive_texture(std::move(other.m_emissive_texture)),
      m_metallic_roughness_texture(std::move(other.m_metallic_roughness_texture)),
      m_normal_texture(std::move(other.m_normal_texture)),
      m_ambient_occlusion_texture(std::move(other.m_ambient_occlusion_texture)),
      m_opacity_texture(std::move(other.m_opacity_texture))
{
    other.m_bit_mask = 0;
}

Rndr::Material& Rndr::Material::operator=(Material&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    m_desc = std::move(other.m_desc);
    m_bit_mask = other.m_bit_mask;
    m_graphics_context = std::move(other.m_graphics_context);
    m_albedo_texture = std::move(other.m_albedo_texture);
    m_emissive_texture = std::move(other.m_emissive_texture);
    m_metallic_roughness_texture = std::move(other.m_metallic_roughness_texture);
    m_normal_texture = std::move(other.m_normal_texture);
    m_ambient_occlusion_texture = std::move(other.m_ambient_occlusion_texture);
    m_opacity_texture = std::move(other.m_opacity_texture);
    other.m_bit_mask = 0;
    return *this;
}

void Rndr::Material::Destroy()
{
    m_albedo_texture.Destroy();
    m_emissive_texture.Destroy();
    m_metallic_roughness_texture.Destroy();
    m_normal_texture.Destroy();
    m_ambient_occlusion_texture.Destroy();
    m_opacity_texture.Destroy();
}

bool Rndr::Material::operator==(const Material& other) const
{
    if (m_bit_mask != other.m_bit_mask)
    {
        return false;
    }
    if ((m_bit_mask & k_bit_mask_albedo_texture) != 0u)
    {
        if (m_desc.albedo_texture_path != other.m_desc.albedo_texture_path)
        {
            return false;
        }
    }
    if ((m_bit_mask & k_bit_mask_emissive_texture) != 0u)
    {
        if (m_desc.emissive_texture_path != other.m_desc.emissive_texture_path)
        {
            return false;
        }
    }
    if ((m_bit_mask & k_bit_mask_metallic_roughness_texture) != 0u)
    {
        if (m_desc.metallic_roughness_texture_path != other.m_desc.metallic_roughness_texture_path)
        {
            return false;
        }
    }
    if ((m_bit_mask & k_bit_mask_normal_texture) != 0u)
    {
        if (m_desc.normal_texture_path != other.m_desc.normal_texture_path)
        {
            return false;
        }
    }
    if ((m_bit_mask & k_bit_mask_ambient_occlusion_texture) != 0u)
    {
        if (m_desc.ambient_occlusion_texture_path != other.m_desc.ambient_occlusion_texture_path)
        {
            return false;
        }
    }
    if ((m_bit_mask & k_bit_mask_opacity_texture) != 0u)
    {
        if (m_desc.opacity_texture_path != other.m_desc.opacity_texture_path)
        {
            return false;
        }
    }
    return true;
}

void Rndr::Material::BindResources(CommandList& command_list) const
{
    if (m_albedo_texture.IsValid())
    {
        command_list.CmdBindTexture(m_albedo_texture, 0);
    }
    if (m_emissive_texture.IsValid())
    {
        command_list.CmdBindTexture(m_emissive_texture, 1);
    }
    if (m_metallic_roughness_texture.IsValid())
    {
        command_list.CmdBindTexture(m_metallic_roughness_texture, 2);
    }
    if (m_normal_texture.IsValid())
    {
        command_list.CmdBindTexture(m_normal_texture, 3);
    }
    if (m_ambient_occlusion_texture.IsValid())
    {
        command_list.CmdBindTexture(m_ambient_occlusion_texture, 4);
    }
    if (m_opacity_texture.IsValid())
    {
        command_list.CmdBindTexture(m_opacity_texture, 5);
    }
}

Rndr::Texture Rndr::Material::LoadTexture(const Opal::StringUtf8& texture_path)
{
    if (texture_path.IsEmpty())
    {
        return {};
    }
    if (!Opal::Exists(texture_path))
    {
        throw Opal::Exception("Albedo texture path does not exist");
    }
    Bitmap bitmap = File::LoadImage(texture_path,  true, false);
    if (!bitmap.IsValid())
    {
        throw Opal::Exception("Failed to load bitmap!");
    }
    const TextureDesc image_desc{.width = bitmap.GetWidth(),
                                 .height = bitmap.GetHeight(),
                                 .array_size = 1,
                                 .type = TextureType::Texture2D,
                                 .pixel_format = bitmap.GetPixelFormat(),
                                 .use_mips = true};
    const SamplerDesc sampler_desc{.max_anisotropy = 16.0f, .border_color = Colors::k_black};
    const Opal::ArrayView<const u8> bitmap_data{bitmap.GetData(), bitmap.GetSize3D()};
    Texture texture = Texture{m_graphics_context, image_desc, sampler_desc, bitmap_data};
    if (!texture.IsValid())
    {
        throw Opal::Exception("Failed to create texture!");
    }
    return texture;
}

void Rndr::MaterialRegistry::Register(Opal::StringUtf8 name, const MaterialDesc& desc)
{
    Material material{m_graphics_context, desc};
    m_materials.Insert(Opal::Move(name), Opal::Move(material));
}

Opal::Ref<const Rndr::Material> Rndr::MaterialRegistry::Get(const Opal::StringUtf8& name) const
{
    const Material& material = m_materials.GetValue(name);
    return Opal::Ref(material);
}
