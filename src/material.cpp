#include "rndr/material.hpp"

#include "opal/file-system.h"

#include "rndr/file.hpp"
#include "rndr/graphics-types.hpp"

Rndr::Material::Material(Opal::Ref<GraphicsContext> graphics_context, const MaterialDesc& desc)
    : m_desc(desc), m_graphics_context(graphics_context)
{
    if (Opal::Exists(m_desc.albedo_texture_path))
    {
        Bitmap bitmap = File::ReadEntireImage(m_desc.albedo_texture_path, PixelFormat::R8G8B8A8_UNORM, true);
        RNDR_ASSERT(bitmap.IsValid(), "Failed to load bitmap!");
        const TextureDesc image_desc{.width = bitmap.GetWidth(),
                                     .height = bitmap.GetHeight(),
                                     .array_size = 1,
                                     .type = TextureType::Texture2D,
                                     .pixel_format = bitmap.GetPixelFormat(),
                                     .use_mips = true};
        const SamplerDesc sampler_desc{.max_anisotropy = 16.0f, .border_color = Colors::k_black};
        const Opal::ArrayView<const u8> bitmap_data{bitmap.GetData(), bitmap.GetSize3D()};
        m_albedo_texture = Texture{m_graphics_context, image_desc, sampler_desc, bitmap_data};
        RNDR_ASSERT(m_albedo_texture.IsValid(), "Failed to create texture!");
        m_bit_mask |= k_bit_mask_albedo_texture;
    }
    else
    {
        m_bit_mask &= ~k_bit_mask_albedo_texture;
    }
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
    return true;
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
