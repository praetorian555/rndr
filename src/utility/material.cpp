#include "rndr/utility/material.h"
#include "rndr/core/containers/hash-map.h"
#include "rndr/core/containers/stack-array.h"

#include <algorithm>
#include <execution>

#include <assimp/material.h>
#include <assimp/pbrmaterial.h>

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

bool Rndr::ReadMaterialDescription(MaterialDescription& out_description, Array<String>& out_texture_paths, Array<String>& out_opacity_maps,
                                   const aiMaterial& ai_material)
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
    if (aiGetMaterialTexture(&ai_material, aiTextureType_NORMALS, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        out_description.metallic_roughness_map = AddUnique(out_texture_paths, out_texture_path.C_Str());
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

    return true;
}

bool Rndr::ConvertAndDownscaleTextures(const std::vector<MaterialDescription>& materials, const String& base_path,
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

    auto converter = [&](const String& s) -> String
    { return ConvertTexture(s, base_path, albedo_map_path_to_opacity_map_index, opacity_maps); };

    std::transform(std::execution::par, texture_paths.cbegin(), texture_paths.cend(), texture_paths.begin(), converter);
    return true;
}

namespace
{
Rndr::String ConvertTexture(const Rndr::String& texture_path, const Rndr::String& base_path,
                            Rndr::HashMap<Rndr::String, uint64_t>& albedo_map_path_to_opacity_map_index,
                            const Rndr::Array<Rndr::String>& opacity_maps)
{
    RNDR_UNUSED(texture_path);
    RNDR_UNUSED(base_path);
    RNDR_UNUSED(albedo_map_path_to_opacity_map_index);
    RNDR_UNUSED(opacity_maps);
    // TODO: Implement
    return "";
}
} // namespace
