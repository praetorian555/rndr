#include "rndr/utility/material.h"

#include <execution>
#include <filesystem>

#include "rndr/core/containers/hash-map.h"
#include "rndr/core/file.h"

#include "stb_image/stb_image.h"
#include "stb_image/stb_image_resize2.h"
#include "stb_image/stb_image_write.h"

namespace
{
Rndr::String ConvertTexture(const Rndr::String& texture_path, const Rndr::String& base_path,
                            Rndr::HashMap<Rndr::String, uint64_t>& albedo_texture_path_to_opacity_texture_index,
                            const Rndr::Array<Rndr::String>& opacity_textures);
bool SetupMaterial(Rndr::MaterialDescription& in_out_material, Rndr::Array<Rndr::Image>& out_textures,
                   const Rndr::GraphicsContext& graphics_context, const Rndr::Array<Rndr::String>& in_texture_paths);
Rndr::Image LoadTexture(const Rndr::GraphicsContext& graphics_context, const Rndr::String& texture_path);
}  // namespace

bool Rndr::Material::ConvertAndDownscaleTextures(const Array<MaterialDescription>& materials, const String& base_path,
                                                 Array<String>& texture_paths, const Array<String>& opacity_textures)
{
    HashMap<String, uint64_t> albedo_map_path_to_opacity_map_index(texture_paths.size());
    for (const MaterialDescription& mat_desc : materials)
    {
        if (mat_desc.opacity_texture != k_invalid_image_id && mat_desc.albedo_texture != k_invalid_image_id)
        {
            albedo_map_path_to_opacity_map_index[texture_paths[static_cast<size_t>(mat_desc.albedo_texture)]] = mat_desc.opacity_texture;
        }
    }

    auto converter = [&](const String& s) { return ConvertTexture(s, base_path, albedo_map_path_to_opacity_map_index, opacity_textures); };

    std::transform(std::execution::par, texture_paths.cbegin(), texture_paths.cend(), texture_paths.begin(), converter);
    return true;
}

bool Rndr::Material::WriteData(const Rndr::Array<Rndr::MaterialDescription>& materials, const Rndr::Array<Rndr::String>& texture_paths,
                               const Rndr::String& file_path)
{
    FILE* ff = nullptr;
    fopen_s(&ff, file_path.c_str(), "wb");
    if (ff == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file %s!", file_path.c_str());
        return false;
    }
    ScopeFilePtr f(ff);

    uint32_t texture_paths_count = static_cast<uint32_t>(texture_paths.size());
    fwrite(&texture_paths_count, 1, sizeof(uint32_t), f.get());
    for (const String& texture_path : texture_paths)
    {
        uint32_t texture_path_length = static_cast<uint32_t>(texture_path.size());
        fwrite(&texture_path_length, 1, sizeof(uint32_t), f.get());
        fwrite(texture_path.c_str(), 1, texture_path_length, f.get());
    }

    const uint32_t materials_count = static_cast<uint32_t>(materials.size());
    fwrite(&materials_count, 1, sizeof(uint32_t), f.get());
    fwrite(materials.data(), sizeof(MaterialDescription), materials_count, f.get());

    return true;
}

bool Rndr::Material::ReadDataLoadTextures(Array<Rndr::MaterialDescription>& out_materials, Array<Rndr::Image>& out_textures,
                                          const Rndr::String& file_path, const Rndr::GraphicsContext& graphics_context)
{
    FILE* ff = nullptr;
    fopen_s(&ff, file_path.c_str(), "rb");
    if (ff == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file %s!", file_path.c_str());
        return false;
    }
    ScopeFilePtr f(ff);

    uint32_t texture_paths_count = 0;
    if (fread(&texture_paths_count, 1, sizeof(uint32_t), f.get()) != sizeof(uint32_t))
    {
        RNDR_LOG_ERROR("Failed to read texture paths count!");
        return false;
    }

    std::filesystem::path base_path(file_path);
    base_path = base_path.parent_path();
    Array<String> texture_paths(texture_paths_count);
    for (uint32_t i = 0; i < texture_paths_count; ++i)
    {
        uint32_t texture_path_length = 0;
        if (fread(&texture_path_length, 1, sizeof(uint32_t), f.get()) != sizeof(uint32_t))
        {
            RNDR_LOG_ERROR("Failed to read texture path length!");
            return false;
        }

        texture_paths[i].resize(texture_path_length);
        if (fread(texture_paths[i].data(), 1, texture_path_length, f.get()) != texture_path_length)
        {
            RNDR_LOG_ERROR("Failed to read texture path!");
            return false;
        }

        texture_paths[i] = base_path.string() + "\\" + texture_paths[i];
    }

    uint32_t materials_count = 0;
    if (fread(&materials_count, 1, sizeof(uint32_t), f.get()) != sizeof(uint32_t))
    {
        RNDR_LOG_ERROR("Failed to read materials count!");
        return false;
    }

    out_materials.resize(materials_count);
    if (fread(out_materials.data(), sizeof(MaterialDescription), materials_count, f.get()) != materials_count)
    {
        RNDR_LOG_ERROR("Failed to read materials!");
        return false;
    }

    for (MaterialDescription& material : out_materials)
    {
        if (!SetupMaterial(material, out_textures, graphics_context, texture_paths))
        {
            RNDR_LOG_ERROR("Failed to setup material!");
            return false;
        }
    }

    return true;
}

namespace
{
bool SetupMaterial(Rndr::MaterialDescription& in_out_material, Rndr::Array<Rndr::Image>& out_textures,
                   const Rndr::GraphicsContext& graphics_context, const Rndr::Array<Rndr::String>& in_texture_paths)
{
    if (in_out_material.albedo_texture != Rndr::k_invalid_image_id)
    {
        const Rndr::String& albedo_map_path = in_texture_paths[static_cast<size_t>(in_out_material.albedo_texture)];
        Rndr::Image albedo_map = LoadTexture(graphics_context, albedo_map_path);
        if (!albedo_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load albedo map: %s", albedo_map_path.c_str());
            return false;
        }
        in_out_material.albedo_texture = albedo_map.GetBindlessHandle();
        out_textures.emplace_back(std::move(albedo_map));
    }
    if (in_out_material.metallic_roughness_texture != Rndr::k_invalid_image_id)
    {
        const Rndr::String& metallic_roughness_map_path = in_texture_paths[static_cast<size_t>(in_out_material.metallic_roughness_texture)];
        Rndr::Image metallic_roughness_map = LoadTexture(graphics_context, metallic_roughness_map_path);
        if (!metallic_roughness_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load metallic roughness map: %s", metallic_roughness_map_path.c_str());
            return false;
        }
        in_out_material.metallic_roughness_texture = metallic_roughness_map.GetBindlessHandle();
        out_textures.emplace_back(std::move(metallic_roughness_map));
    }
    if (in_out_material.normal_texture != Rndr::k_invalid_image_id)
    {
        const Rndr::String& normal_map_path = in_texture_paths[static_cast<size_t>(in_out_material.normal_texture)];
        Rndr::Image normal_map = LoadTexture(graphics_context, normal_map_path);
        if (!normal_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load normal map: %s", normal_map_path.c_str());
            return false;
        }
        in_out_material.normal_texture = normal_map.GetBindlessHandle();
        out_textures.emplace_back(std::move(normal_map));
    }
    if (in_out_material.ambient_occlusion_texture != Rndr::k_invalid_image_id)
    {
        const Rndr::String& ambient_occlusion_map_path = in_texture_paths[static_cast<size_t>(in_out_material.ambient_occlusion_texture)];
        Rndr::Image ambient_occlusion_map = LoadTexture(graphics_context, ambient_occlusion_map_path);
        if (!ambient_occlusion_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load ambient occlusion map: %s", ambient_occlusion_map_path.c_str());
            return false;
        }
        in_out_material.ambient_occlusion_texture = ambient_occlusion_map.GetBindlessHandle();
        out_textures.emplace_back(std::move(ambient_occlusion_map));
    }
    if (in_out_material.emissive_texture != Rndr::k_invalid_image_id)
    {
        const Rndr::String& emissive_map_path = in_texture_paths[static_cast<size_t>(in_out_material.emissive_texture)];
        Rndr::Image emissive_map = LoadTexture(graphics_context, emissive_map_path);
        if (!emissive_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load emissive map: %s", emissive_map_path.c_str());
            return false;
        }
        in_out_material.emissive_texture = emissive_map.GetBindlessHandle();
        out_textures.emplace_back(std::move(emissive_map));
    }
    return true;
}

Rndr::String ConvertTexture(const Rndr::String& texture_path, const Rndr::String& base_path,
                            Rndr::HashMap<Rndr::String, uint64_t>& albedo_texture_path_to_opacity_texture_index,
                            const Rndr::Array<Rndr::String>& opacity_textures)
{
    constexpr int32_t k_max_new_width = 512;
    constexpr int32_t k_max_new_height = 512;

    const Rndr::String src_file = base_path + "\\" + texture_path;
    const std::filesystem::path relative_src_path(texture_path);
    const std::filesystem::path src_path(src_file);
    const Rndr::String relative_dst_file = (relative_src_path.parent_path() / relative_src_path.stem()).string() + "_rescaled.png";
    const Rndr::String dst_file = (src_path.parent_path() / src_path.stem()).string() + "_rescaled.png";

    RNDR_LOG_DEBUG("ConvertTexture: %s -> %s", src_file.c_str(), dst_file.c_str());

    // load this image
    int32_t src_width = 0;
    int32_t src_height = 0;
    int32_t src_channels = 0;
    stbi_uc* src_pixels = stbi_load(src_file.c_str(), &src_width, &src_height, &src_channels, STBI_rgb_alpha);
    uint8_t* src_data = src_pixels;
    src_channels = STBI_rgb_alpha;

    Rndr::ByteArray empty_image(k_max_new_width * k_max_new_height * 4);
    if (src_data == nullptr)
    {
        RNDR_LOG_DEBUG("ConvertTexture: Failed to load [%s] texture", src_file.c_str());
        src_width = k_max_new_width;
        src_height = k_max_new_height;
        src_channels = STBI_rgb_alpha;
        src_data = empty_image.data();
    }

    // Check if there is an opacity map for this texture and if there is put the opacity values into the alpha channel
    // of this image.
    if (albedo_texture_path_to_opacity_texture_index.contains(texture_path))
    {
        const uint64_t opacity_map_index = albedo_texture_path_to_opacity_texture_index[texture_path];
        const Rndr::String opacity_map_file = base_path + opacity_textures[opacity_map_index];
        int32_t opacity_width = 0;
        int32_t opacity_height = 0;
        stbi_uc* opacity_pixels = stbi_load(opacity_map_file.c_str(), &opacity_width, &opacity_height, nullptr, 1);
        if (opacity_pixels == nullptr)
        {
            RNDR_LOG_WARNING("ConvertTexture: Failed to load opacity map [%s] for [%s] texture", opacity_map_file.c_str(),
                             src_file.c_str());
        }
        if (opacity_width != src_width || opacity_height != src_height)
        {
            RNDR_LOG_WARNING("ConvertTexture: Opacity map [%s] has different size than [%s] texture", opacity_map_file.c_str(),
                             src_file.c_str());
        }

        // store the opacity mask in the alpha component of this image
        if (opacity_pixels != nullptr && opacity_width == src_width && opacity_height == src_height)
        {
            for (int y = 0; y != opacity_height; y++)
            {
                for (int x = 0; x != opacity_width; x++)
                {
                    src_data[(y * opacity_width + x) * src_channels + 3] = opacity_pixels[y * opacity_width + x];
                }
            }
        }
        else
        {
            RNDR_LOG_WARNING("ConvertTexture: Skipping opacity map [%s] for [%s] texture", opacity_map_file.c_str(), src_file.c_str());
        }

        stbi_image_free(opacity_pixels);
    }

    const uint32_t dst_image_size = src_width * src_height * src_channels;
    Rndr::Array<uint8_t> dst_data(dst_image_size);
    uint8_t* dst = dst_data.data();

    const int dst_width = std::min(src_width, k_max_new_width);
    const int dst_height = std::min(src_height, k_max_new_height);

    if (stbir_resize_uint8_linear(src_data, src_width, src_height, 0, dst, dst_width, dst_height, 0,
                                  static_cast<stbir_pixel_layout>(src_channels)) == nullptr)
    {
        RNDR_LOG_ERROR("ConvertTexture: Failed to resize [%s] texture", src_file.c_str());
        goto cleanup;
    }

    if (stbi_write_png(dst_file.c_str(), dst_width, dst_height, src_channels, dst, 0) == 0)
    {
        RNDR_LOG_ERROR("ConvertTexture: Failed to write [%s] texture", dst_file.c_str());
        goto cleanup;
    }

cleanup:
    if (src_pixels != nullptr)
    {
        stbi_image_free(src_pixels);
    }

    return relative_dst_file;
}

Rndr::Image LoadTexture(const Rndr::GraphicsContext& graphics_context, const Rndr::String& texture_path)
{
    constexpr bool k_flip_vertically = true;
    Rndr::Bitmap bitmap = Rndr::File::ReadEntireImage(texture_path, Rndr::PixelFormat::R8G8B8A8_UNORM, k_flip_vertically);
    if (!bitmap.IsValid())
    {
        RNDR_LOG_ERROR("Failed to load texture from file: %s", texture_path.c_str());
        return {};
    }
    const Rndr::ImageDesc image_desc{.width = bitmap.GetWidth(),
                                     .height = bitmap.GetHeight(),
                                     .array_size = 1,
                                     .type = Rndr::ImageType::Image2D,
                                     .pixel_format = bitmap.GetPixelFormat(),
                                     .use_mips = true,
                                     .is_bindless = true,
                                     .sampler = {.max_anisotropy = 16.0f, .border_color = Rndr::Colors::k_white}};
    const Rndr::ConstByteSpan bitmap_data{bitmap.GetData(), bitmap.GetSize3D()};
    return {graphics_context, image_desc, bitmap_data};
}

}  // namespace
