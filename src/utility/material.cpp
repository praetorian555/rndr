#include "rndr/utility/material.h"
#include "rndr/core/containers/hash-map.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/file.h"
#include "stb_image/stb_image.h"
#include "stb_image/stb_image_resize2.h"
#include "stb_image/stb_image_write.h"

#include <execution>

#include <assimp/material.h>
#include <assimp/pbrmaterial.h>
#include <filesystem>

namespace
{
Rndr::ImageId AddUnique(Rndr::Array<Rndr::String>& files, const char* path)
{
    for (uint32_t i = 0; i < files.size(); ++i)
    {
        if (files[i] == path)
        {
            return i;
        }
    }
    files.emplace_back(path);
    return files.size() - 1;
}

Rndr::String ConvertTexture(const Rndr::String& texture_path, const Rndr::String& base_path,
                            Rndr::HashMap<Rndr::String, uint64_t>& albedo_map_path_to_opacity_map_index,
                            const Rndr::Array<Rndr::String>& opacity_maps);

}  // namespace

bool Rndr::Material::ReadDescription(MaterialDescription& out_description, Array<String>& out_texture_paths,
                                     Array<String>& out_opacity_maps, const aiMaterial& ai_material)
{
    aiColor4D ai_color;
    if (aiGetMaterialColor(&ai_material, AI_MATKEY_COLOR_AMBIENT, &ai_color) == AI_SUCCESS)
    {
        out_description.emissive_color = Vector4f(ai_color.r, ai_color.g, ai_color.b, ai_color.a);
        out_description.emissive_color.a = Math::Clamp(out_description.emissive_color.a, 0.0f, 1.0f);
    }
    if (aiGetMaterialColor(&ai_material, AI_MATKEY_COLOR_EMISSIVE, &ai_color) == AI_SUCCESS)
    {
        out_description.emissive_color.r += ai_color.r;
        out_description.emissive_color.g += ai_color.g;
        out_description.emissive_color.b += ai_color.b;
        out_description.emissive_color.a += ai_color.a;
        out_description.emissive_color.a = Math::Clamp(out_description.emissive_color.a, 0.0f, 1.0f);
    }
    if (aiGetMaterialColor(&ai_material, AI_MATKEY_COLOR_DIFFUSE, &ai_color) == AI_SUCCESS)
    {
        out_description.albedo_color = Vector4f(ai_color.r, ai_color.g, ai_color.b, ai_color.a);
        out_description.albedo_color.a = Math::Clamp(out_description.albedo_color.a, 0.0f, 1.0f);
    }

    // Read opacity factor from the AI material and convert it to transparency factor. If opacity is 95% or more, the material is considered
    // opaque.
    constexpr float k_opaqueness_threshold = 0.05f;
    float opacity = 1.0f;
    if (aiGetMaterialFloat(&ai_material, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS)
    {
        out_description.transparency_factor = 1.0f - opacity;
        out_description.transparency_factor = Math::Clamp(out_description.transparency_factor, 0.0f, 1.0f);
        if (out_description.transparency_factor >= 1.0f - k_opaqueness_threshold)
        {
            out_description.transparency_factor = 0.0f;
        }
    }

    // If AI material contains transparency factor as an RGB value, it will take precedence over the opacity factor.
    if (aiGetMaterialColor(&ai_material, AI_MATKEY_COLOR_TRANSPARENT, &ai_color) == AI_SUCCESS)
    {
        opacity = Math::Max(Math::Max(ai_color.r, ai_color.g), ai_color.b);
        out_description.transparency_factor = Math::Clamp(opacity, 0.0f, 1.0f);
        if (out_description.transparency_factor >= 1.0f - k_opaqueness_threshold)
        {
            out_description.transparency_factor = 0.0f;
        }
        out_description.alpha_test = 0.5f;
    }

    // Read roughness and metallic factors from the AI material.
    float factor = 1.0f;
    if (aiGetMaterialFloat(&ai_material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, &factor) == AI_SUCCESS)
    {
        out_description.metallic_factor = factor;
    }
    if (aiGetMaterialFloat(&ai_material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, &factor) == AI_SUCCESS)
    {
        out_description.roughness = Vector4f(factor, factor, 0.0f, 0.0f);
    }

    // Get info about the texture file paths, store them in the out_texture_paths array and set the corresponding image ids in the material
    // description.
    aiString out_texture_path;
    aiTextureMapping out_texture_mapping = aiTextureMapping_UV;
    unsigned int out_uv_index = 0;
    float out_blend = 1.0f;
    aiTextureOp out_texture_op = aiTextureOp_Add;
    StackArray<aiTextureMapMode, 2> out_texture_mode = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
    unsigned int out_texture_flags = 0;
    if (aiGetMaterialTexture(&ai_material, aiTextureType_EMISSIVE, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        out_description.emissive_map = AddUnique(out_texture_paths, out_texture_path.C_Str());
    }
    if (aiGetMaterialTexture(&ai_material, aiTextureType_DIFFUSE, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        out_description.albedo_map = AddUnique(out_texture_paths, out_texture_path.C_Str());
        // Some material heuristics
        const String albedo_map_path = out_texture_path.C_Str();
        if (albedo_map_path.find("grey_30") != String::npos)
        {
            out_description.flags |= MaterialFlags::Transparent;
        }
    }
    if (aiGetMaterialTexture(&ai_material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &out_texture_path,
                             &out_texture_mapping, &out_uv_index, &out_blend, &out_texture_op, out_texture_mode.data(),
                             &out_texture_flags) == AI_SUCCESS)
    {
        out_description.metallic_roughness_map = AddUnique(out_texture_paths, out_texture_path.C_Str());
    }
    if (aiGetMaterialTexture(&ai_material, aiTextureType_LIGHTMAP, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        out_description.ambient_occlusion_map = AddUnique(out_texture_paths, out_texture_path.C_Str());
    }
    if (aiGetMaterialTexture(&ai_material, aiTextureType_NORMALS, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        out_description.normal_map = AddUnique(out_texture_paths, out_texture_path.C_Str());
    }
    // In case that there is no normal map, try to read the height map that can be later converted into a normal map.
    if (out_description.normal_map == k_invalid_image_id)
    {
        if (aiGetMaterialTexture(&ai_material, aiTextureType_HEIGHT, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                                 &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
        {
            out_description.normal_map = AddUnique(out_texture_paths, out_texture_path.C_Str());
        }
    }
    if (aiGetMaterialTexture(&ai_material, aiTextureType_OPACITY, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        // Opacity info will later be stored in the alpha channel of the albedo map.
        out_description.opacity_map = AddUnique(out_opacity_maps, out_texture_path.C_Str());
        out_description.alpha_test = 0.5f;
    }

    // Material heuristics, modify material parameters based on the texture name so that it looks better.
    aiString ai_material_name;
    String material_name;
    if (aiGetMaterialString(&ai_material, AI_MATKEY_NAME, &ai_material_name) == AI_SUCCESS)
    {
        material_name = ai_material_name.C_Str();
    }
    if ((material_name.find("Glass") != String::npos) || (material_name.find("Vespa_Headlight") != String::npos))
    {
        out_description.alpha_test = 0.75f;
        out_description.transparency_factor = 0.1f;
        out_description.flags |= MaterialFlags::Transparent;
    }
    else if (material_name.find("Bottle") != String::npos)
    {
        out_description.alpha_test = 0.54f;
        out_description.transparency_factor = 0.4f;
        out_description.flags |= MaterialFlags::Transparent;
    }
    else if (material_name.find("Metal") != String::npos)
    {
        out_description.metallic_factor = 1.0f;
        out_description.roughness = Vector4f(0.1f, 0.1f, 0.0f, 0.0f);
    }

    RNDR_LOG_DEBUG("Texture paths: %d", out_texture_paths.size());
    for (const String& texture_path : out_texture_paths)
    {
        RNDR_LOG_DEBUG("\t%s", texture_path.c_str());
    }

    RNDR_LOG_DEBUG("Opacity maps: %d", out_opacity_maps.size());
    for (const String& out_opacity_map : out_opacity_maps)
    {
        RNDR_LOG_DEBUG("\t%s", out_opacity_map.c_str());
    }

    return true;
}

bool Rndr::Material::ConvertAndDownscaleTextures(const Array<MaterialDescription>& materials, const String& base_path,
                                                 Array<String>& texture_paths, const Array<String>& opacity_maps)
{
    HashMap<String, uint64_t> albedo_map_path_to_opacity_map_index(texture_paths.size());
    for (const MaterialDescription& mat_desc : materials)
    {
        if (mat_desc.opacity_map != k_invalid_image_id && mat_desc.albedo_map != k_invalid_image_id)
        {
            albedo_map_path_to_opacity_map_index[texture_paths[static_cast<size_t>(mat_desc.albedo_map)]] = mat_desc.opacity_map;
        }
    }

    auto converter = [&](const String& s) { return ConvertTexture(s, base_path, albedo_map_path_to_opacity_map_index, opacity_maps); };

    std::transform(std::execution::par, texture_paths.cbegin(), texture_paths.cend(), texture_paths.begin(), converter);
    return true;
}

bool Rndr::Material::WriteOptimizedData(const Rndr::Array<Rndr::MaterialDescription>& materials,
                                        const Rndr::Array<Rndr::String>& texture_paths, const Rndr::String& file_path)
{
    FILE* f = nullptr;
    fopen_s(&f, file_path.c_str(), "wb");
    if (f == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file %s!", file_path.c_str());
        return false;
    }

    uint32_t texture_paths_count = static_cast<uint32_t>(texture_paths.size());
    fwrite(&texture_paths_count, 1, sizeof(uint32_t), f);
    for (const String& texture_path : texture_paths)
    {
        uint32_t texture_path_length = static_cast<uint32_t>(texture_path.size());
        fwrite(&texture_path_length, 1, sizeof(uint32_t), f);
        fwrite(texture_path.c_str(), 1, texture_path_length, f);
    }

    const uint32_t materials_count = static_cast<uint32_t>(materials.size());
    fwrite(&materials_count, 1, sizeof(uint32_t), f);
    fwrite(materials.data(), sizeof(MaterialDescription), materials_count, f);

    fclose(f);
    return true;
}

bool Rndr::Material::ReadOptimizedData(Rndr::Array<Rndr::MaterialDescription>& out_materials, Rndr::Array<Rndr::String>& out_texture_paths,
                                       const Rndr::String& file_path)
{
    FILE* f = nullptr;
    fopen_s(&f, file_path.c_str(), "rb");
    if (f == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file %s!", file_path.c_str());
        return false;
    }

    uint32_t texture_paths_count = 0;
    if (fread(&texture_paths_count, 1, sizeof(uint32_t), f) != sizeof(uint32_t))
    {
        RNDR_LOG_ERROR("Failed to read texture paths count!");
        fclose(f);
        return false;
    }

    std::filesystem::path base_path(file_path);
    base_path = base_path.parent_path();
    out_texture_paths.resize(texture_paths_count);
    for (uint32_t i = 0; i < texture_paths_count; ++i)
    {
        uint32_t texture_path_length = 0;
        if (fread(&texture_path_length, 1, sizeof(uint32_t), f) != sizeof(uint32_t))
        {
            RNDR_LOG_ERROR("Failed to read texture path length!");
            fclose(f);
            return false;
        }

        out_texture_paths[i].resize(texture_path_length);
        if (fread(out_texture_paths[i].data(), 1, texture_path_length, f) != texture_path_length)
        {
            RNDR_LOG_ERROR("Failed to read texture path!");
            fclose(f);
            return false;
        }

        out_texture_paths[i] = base_path.string() + "\\" + out_texture_paths[i];
    }

    uint32_t materials_count = 0;
    if (fread(&materials_count, 1, sizeof(uint32_t), f) != sizeof(uint32_t))
    {
        RNDR_LOG_ERROR("Failed to read materials count!");
        fclose(f);
        return false;
    }

    out_materials.resize(materials_count);
    if (fread(out_materials.data(), sizeof(MaterialDescription), materials_count, f) != materials_count)
    {
        RNDR_LOG_ERROR("Failed to read materials!");
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

namespace
{
Rndr::Image LoadTexture(const Rndr::GraphicsContext& graphics_context, const Rndr::String& texture_path);
}

bool Rndr::Material::SetupMaterial(Rndr::MaterialDescription& in_out_material, Rndr::Array<Rndr::Image>& out_textures,
                                   const Rndr::GraphicsContext& graphics_context, const Rndr::Array<Rndr::String>& in_texture_paths)
{
    if (in_out_material.albedo_map != k_invalid_image_id)
    {
        const Rndr::String& albedo_map_path = in_texture_paths[static_cast<size_t>(in_out_material.albedo_map)];
        Rndr::Image albedo_map = LoadTexture(graphics_context, albedo_map_path);
        if (!albedo_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load albedo map: %s", albedo_map_path.c_str());
            return false;
        }
        in_out_material.albedo_map = albedo_map.GetBindlessHandle();
        out_textures.emplace_back(std::move(albedo_map));
    }
    if (in_out_material.metallic_roughness_map != k_invalid_image_id)
    {
        const Rndr::String& metallic_roughness_map_path = in_texture_paths[static_cast<size_t>(in_out_material.metallic_roughness_map)];
        Rndr::Image metallic_roughness_map = LoadTexture(graphics_context, metallic_roughness_map_path);
        if (!metallic_roughness_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load metallic roughness map: %s", metallic_roughness_map_path.c_str());
            return false;
        }
        in_out_material.metallic_roughness_map = metallic_roughness_map.GetBindlessHandle();
        out_textures.emplace_back(std::move(metallic_roughness_map));
    }
    if (in_out_material.normal_map != k_invalid_image_id)
    {
        const Rndr::String& normal_map_path = in_texture_paths[static_cast<size_t>(in_out_material.normal_map)];
        Rndr::Image normal_map = LoadTexture(graphics_context, normal_map_path);
        if (!normal_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load normal map: %s", normal_map_path.c_str());
            return false;
        }
        in_out_material.normal_map = normal_map.GetBindlessHandle();
        out_textures.emplace_back(std::move(normal_map));
    }
    if (in_out_material.ambient_occlusion_map != k_invalid_image_id)
    {
        const Rndr::String& ambient_occlusion_map_path = in_texture_paths[static_cast<size_t>(in_out_material.ambient_occlusion_map)];
        Rndr::Image ambient_occlusion_map = LoadTexture(graphics_context, ambient_occlusion_map_path);
        if (!ambient_occlusion_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load ambient occlusion map: %s", ambient_occlusion_map_path.c_str());
            return false;
        }
        in_out_material.ambient_occlusion_map = ambient_occlusion_map.GetBindlessHandle();
        out_textures.emplace_back(std::move(ambient_occlusion_map));
    }
    if (in_out_material.emissive_map != k_invalid_image_id)
    {
        const Rndr::String& emissive_map_path = in_texture_paths[static_cast<size_t>(in_out_material.emissive_map)];
        Rndr::Image emissive_map = LoadTexture(graphics_context, emissive_map_path);
        if (!emissive_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load emissive map: %s", emissive_map_path.c_str());
            return false;
        }
        in_out_material.emissive_map = emissive_map.GetBindlessHandle();
        out_textures.emplace_back(std::move(emissive_map));
    }
    return true;
}

namespace
{
Rndr::String ConvertTexture(const Rndr::String& texture_path, const Rndr::String& base_path,
                            Rndr::HashMap<Rndr::String, uint64_t>& albedo_map_path_to_opacity_map_index,
                            const Rndr::Array<Rndr::String>& opacity_maps)
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
    if (albedo_map_path_to_opacity_map_index.contains(texture_path))
    {
        const uint64_t opacity_map_index = albedo_map_path_to_opacity_map_index[texture_path];
        const Rndr::String opacity_map_file = base_path + opacity_maps[opacity_map_index];
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
