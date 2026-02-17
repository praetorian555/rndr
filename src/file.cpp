#include "rndr/file.hpp"

#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

#if RNDR_ADVANCED_API
#include "ktx.h"
#include "ktxvulkan.h"
#endif

#include "assimp/cimport.h"
#include "assimp/pbrmaterial.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "opal/container/in-place-array.h"
#include "opal/file-system.h"
#include "opal/paths.h"
#include "opal/exceptions.h"

#include "rndr/log.hpp"

Rndr::FileHandler::FileHandler(const char* file_path, const char* mode)
{
    fopen_s(&m_file_handle, file_path, mode);
}

Rndr::FileHandler::~FileHandler()
{
    if (m_file_handle != 0)
    {
        fclose(m_file_handle);
    }
}

bool Rndr::FileHandler::IsValid() const
{
    return m_file_handle != 0;
}

bool Rndr::FileHandler::IsEOF() const
{
    return feof(m_file_handle) != 0;
}

bool Rndr::FileHandler::Read(void* buffer, u64 element_size, u64 element_count)
{
    const u64 read_elements = fread(buffer, element_size, element_count, m_file_handle);
    RNDR_ASSERT(read_elements == element_count, "Expected to read element_count elements!");
    return read_elements == element_count;
}

bool Rndr::FileHandler::Write(const void* buffer, u64 element_size, u64 element_count)
{
    const u64 written_elements = fwrite(buffer, element_size, element_count, m_file_handle);
    RNDR_ASSERT(written_elements == element_count, "Expected to write element_count elements!");
    return written_elements == element_count;
}

Opal::DynamicArray<Rndr::u8> Rndr::File::ReadEntireFile(const Opal::StringUtf8& file_path)
{
    Opal::StringLocale file_path_locale;
    file_path_locale.Resize(300);
    const Opal::ErrorCode err = Opal::Transcode(file_path, file_path_locale);
    if (err != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("Failed to transcode file path!");
        return {};
    }
    FILE* file = nullptr;
    fopen_s(&file, file_path_locale.GetData(), "rb");
    if (file == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file %s", file_path.GetData());
        return {};
    }

    fseek(file, 0, SEEK_END);
    const int contents_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    Opal::DynamicArray<u8> contents(contents_size);
    const u64 read_bytes = fread(contents.GetData(), 1, contents.GetSize(), file);
    if (read_bytes != contents_size)
    {
        RNDR_LOG_WARNING("Failed to read all bytes from the file!");
    }

    fclose(file);

    return contents;
}

Opal::StringUtf8 Rndr::File::ReadEntireTextFile(const Opal::StringUtf8& file_path)
{
    Opal::DynamicArray<u8> contents = ReadEntireFile(file_path);
    return {reinterpret_cast<char8*>(contents.GetData()), contents.GetSize()};
}

Opal::StringUtf8 Rndr::File::ReadShader(const Opal::StringUtf8& ref_path, const Opal::StringUtf8& shader_path)
{
    const Opal::StringUtf8 full_path = Opal::Paths::Combine(ref_path, shader_path);
    if (!Opal::Exists(full_path))
    {
        RNDR_LOG_ERROR("Shader file %s does not exist!", full_path.GetData());
        return {};
    }

    Opal::StringUtf8 shader_contents = ReadEntireTextFile(full_path);
    if (shader_contents.IsEmpty())
    {
        RNDR_LOG_ERROR("Failed to read shader file %s!", full_path.GetData());
        return {};
    }

    static constexpr u8 k_bom[] = {0xEF, 0xBB, 0xBF};
    if (shader_contents.GetSize() > sizeof(k_bom) && memcmp(shader_contents.GetData(), k_bom, sizeof(k_bom)) == 0)
    {
        shader_contents.Erase(0, 3);
    }

    while (true)
    {
        u64 const result = Opal::Find(shader_contents, "#include");
        if (result == Opal::StringUtf8::k_npos)
        {
            break;
        }

        const u64 include_start = Opal::Find(shader_contents, "#include");
        const u64 include_end = Opal::Find(shader_contents, "\n", include_start);
        const u64 include_length = include_end - include_start;
        const Opal::StringUtf8 include_line = Opal::GetSubString(shader_contents, include_start, include_length).GetValue();
        const u64 quote_start = Opal::Find(include_line, '\"');
        const u64 quote_end = Opal::ReverseFind(include_line, '\"');
        const u64 quote_length = quote_end - quote_start;
        const u64 bracket_start = Opal::Find(include_line, '<');
        const u64 bracket_end = Opal::ReverseFind(include_line, '>');
        const u64 bracket_length = bracket_end - bracket_start;
        Opal::StringUtf8 include_path;
        if (quote_start != Opal::StringUtf8::k_npos)
        {
            include_path = Opal::GetSubString(include_line, quote_start + 1, quote_length - 1).GetValue();
        }
        else if (bracket_start != Opal::StringUtf8::k_npos)
        {
            include_path = Opal::GetSubString(include_line, bracket_start + 1, bracket_length - 1).GetValue();
        }
        else
        {
            RNDR_LOG_ERROR("Invalid include statement %s", include_line.GetData());
            return {};
        }
        auto parent_path_result = Opal::Paths::GetParentPath(full_path);
        RNDR_ASSERT(parent_path_result.HasValue(), "Shader parent directory path is empty!");
        const Opal::StringUtf8 parent_path = parent_path_result.GetValue();
        const Opal::StringUtf8 include_contents = ReadShader(parent_path, include_path);
        shader_contents.Erase(include_start, include_length);
        shader_contents.Insert(include_start, include_contents);
    }

    return shader_contents;
}

void Rndr::File::PrintShader(const Opal::StringUtf8& shader_contents)
{
    int line_number = 1;
    Opal::StringUtf8 line_buffer;
    for (const auto& c : shader_contents)
    {
        if (c == '\n')
        {
            RNDR_LOG_INFO("%d: %s", line_number, line_buffer.GetData());
            line_buffer.Erase();
            line_number++;
        }
        else if (c == '\r')
        {
            continue;
        }
        else
        {
            line_buffer += c;
        }
    }
}

bool Rndr::File::SaveImage(const Bitmap& bitmap, const Opal::StringUtf8& file_path)
{
    Opal::StringLocale file_path_locale;
    file_path_locale.Resize(300);
    const Opal::ErrorCode err = Opal::Transcode(file_path, file_path_locale);
    if (err != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("Failed to transcode file path!");
        return false;
    }
    int status = 0;
    const PixelFormat pixel_format = bitmap.GetPixelFormat();
    if (IsLowPrecisionFormat(pixel_format))
    {
        status = stbi_write_png(file_path_locale.GetData(), bitmap.GetWidth(), bitmap.GetHeight(), bitmap.GetComponentCount(),
                                bitmap.GetData(), static_cast<int>(bitmap.GetRowSize()));
    }
    else if (IsHighPrecisionFormat(pixel_format))
    {
        const f32* data = reinterpret_cast<const f32*>(bitmap.GetData());
        status = stbi_write_hdr(file_path_locale.GetData(), bitmap.GetWidth(), bitmap.GetHeight(), bitmap.GetComponentCount(), data);
    }
    else
    {
        RNDR_LOG_ERROR("Unsupported pixel format for saving!");
    }
    return status == 1;
}

#if RNDR_ADVANCED_API
namespace
{
Rndr::PixelFormat VkFormatToPixelFormat(ktx_uint32_t vk_format)
{
    return static_cast<Rndr::PixelFormat>(vk_format);
}
}

Rndr::Bitmap Rndr::File::LoadImage(const Opal::StringUtf8& file_path, bool flip_vertically, bool generate_mips)
{
    if (!Opal::Exists(file_path))
    {
        throw Opal::Exception("File does not exist!");
    }

    // Determine file extension.
    const Opal::StringUtf8 extension = Opal::Paths::GetExtension(file_path).GetValue();

    if (extension == ".ktx" || extension == ".ktx2")
    {
        ktxTexture* ktx_texture = nullptr;
        const KTX_error_code result =
            ktxTexture_CreateFromNamedFile(*file_path, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
        if (result != KTX_SUCCESS || ktx_texture == nullptr)
        {
            throw Opal::Exception("Failed to create ktx texture!");
        }

        const ktx_uint32_t vk_format = ktxTexture_GetVkFormat(ktx_texture);
        const PixelFormat pixel_format = VkFormatToPixelFormat(vk_format);
        const i32 width = static_cast<i32>(ktx_texture->baseWidth);
        const i32 height = static_cast<i32>(ktx_texture->baseHeight);
        const i32 depth = static_cast<i32>(ktx_texture->baseDepth);
        const i32 mip_count = static_cast<i32>(ktx_texture->numLevels);
        u8* data = ktxTexture_GetData(ktx_texture);
        const u64 data_size = ktxTexture_GetDataSize(ktx_texture);

        Bitmap bitmap(width, height, depth, pixel_format, mip_count, {data, data_size});
        ktxTexture_Destroy(ktx_texture);

        if (generate_mips && mip_count <= 1)
        {
            bitmap.GenerateMips();
        }

        return bitmap;
    }

    stbi_set_flip_vertically_on_load(flip_vertically);

    int width = 0;
    int height = 0;
    int channels_in_file = 0;
    constexpr int k_desired_channels = 4;

    PixelFormat pixel_format = PixelFormat::Undefined;
    u8* pixel_data = nullptr;
    u64 data_size = 0;

    if (stbi_is_hdr(*file_path) > 0)
    {
        f32* data = stbi_loadf(*file_path, &width, &height, &channels_in_file, k_desired_channels);
        if (data == nullptr)
        {
            throw Opal::Exception("Failed to load HDR image");
        }
        pixel_data = reinterpret_cast<u8*>(data);
        pixel_format = PixelFormat::R32G32B32A32_SFLOAT;
        data_size = static_cast<u64>(width) * height * k_desired_channels * sizeof(f32);
    }
    else if (stbi_is_16_bit(*file_path) > 0)
    {
        u16* data = reinterpret_cast<u16*>(stbi_load_16(*file_path, &width, &height, &channels_in_file, k_desired_channels));
        if (data == nullptr)
        {
            throw Opal::Exception("Failed to load 16-bit image");
        }
        pixel_data = reinterpret_cast<u8*>(data);
        pixel_format = PixelFormat::R16G16B16A16_UNORM;
        data_size = static_cast<u64>(width) * height * k_desired_channels * sizeof(u16);
    }
    else
    {
        u8* data = stbi_load(*file_path, &width, &height, &channels_in_file, k_desired_channels);
        if (data == nullptr)
        {
            throw Opal::Exception("Failed to load image");
        }
        pixel_data = data;
        pixel_format = PixelFormat::R8G8B8A8_SRGB;
        data_size = static_cast<u64>(width) * height * k_desired_channels * sizeof(u8);
    }

    Bitmap bitmap(width, height, 1, pixel_format, 1, {pixel_data, data_size});
    stbi_image_free(pixel_data);

    if (generate_mips)
    {
        bitmap.GenerateMips();
    }

    return bitmap;
}
#endif

void Rndr::File::LoadMeshAndMaterialDescription(const Opal::StringUtf8& file_path, Mesh& out_mesh, MaterialDesc& out_material_desc)
{
    constexpr u32 k_ai_process_flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                       aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                                       aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                       aiProcess_GenUVCoords;
    const Opal::StringUtf8 mesh_name = Opal::Paths::GetFileName(file_path).GetValue();
    const Opal::StringUtf8 mesh_dir = Opal::Paths::GetParentPath(file_path).GetValue();
    const aiScene* scene = aiImportFile(*file_path, k_ai_process_flags);
    if (scene == nullptr)
    {
        throw Opal::Exception("Failed to load a mesh");
    }
    u32 material_index = static_cast<u32>(-1);
    LoadMesh(*scene, mesh_name, out_mesh, material_index);
    if (material_index == static_cast<u32>(-1))
    {
        throw Opal::Exception("Failed to find a material for the mesh");
    }
    LoadMaterialDescription(*scene, material_index, mesh_dir, out_material_desc);
}

void Rndr::File::LoadMesh(const aiScene& ai_scene, const Opal::StringUtf8& mesh_name, Mesh& out_mesh, u32& out_material_index)
{
    if (!ai_scene.HasMeshes())
    {
        throw Opal::Exception("No meshes found!");
    }

    const aiMesh* ai_mesh = ai_scene.mMeshes[0];
    out_mesh.vertex_size = sizeof(Point3f) + sizeof(Normal3f) + sizeof(Point2f);
    out_material_index = ai_mesh->mMaterialIndex;
    out_mesh.name = mesh_name;
    ;
    for (u32 vertex_idx = 0; vertex_idx < ai_mesh->mNumVertices; ++vertex_idx)
    {
        Point3f position(ai_mesh->mVertices[vertex_idx].x, ai_mesh->mVertices[vertex_idx].y, ai_mesh->mVertices[vertex_idx].z);
        Normal3f normal(ai_mesh->mNormals[vertex_idx].x, ai_mesh->mNormals[vertex_idx].y, ai_mesh->mNormals[vertex_idx].z);
        Point2f uv(ai_mesh->mTextureCoords[0][vertex_idx].x, ai_mesh->mTextureCoords[0][vertex_idx].y);
        out_mesh.vertices.Append(Opal::AsWritableBytes(position));
        out_mesh.vertices.Append(Opal::AsWritableBytes(normal));
        out_mesh.vertices.Append(Opal::AsWritableBytes(uv));
        ++out_mesh.vertex_count;
    }

    for (u32 face_idx = 0; face_idx < ai_mesh->mNumFaces; ++face_idx)
    {
        const aiFace& face = ai_mesh->mFaces[face_idx];
        if (face.mNumIndices != 3)
        {
            continue;
        }
        for (u32 index_idx = 0; index_idx < face.mNumIndices; ++index_idx)
        {
            out_mesh.indices.Append(Opal::AsWritableBytes<u32>(face.mIndices[index_idx]));
            ++out_mesh.index_count;
        }
    }
}

void Rndr::File::LoadMaterialDescription(const aiScene& ai_scene, u32 material_index, const Opal::StringUtf8& parent_path,
                                         MaterialDesc& out_material_desc)
{
    const aiMaterial* ai_material = ai_scene.mMaterials[material_index];

    aiColor4D ai_color;
    if (aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_AMBIENT, &ai_color) == AI_SUCCESS)
    {
        out_material_desc.emissive_color = Vector4f(ai_color.r, ai_color.g, ai_color.b, ai_color.a);
        out_material_desc.emissive_color.a = Opal::Clamp(out_material_desc.emissive_color.a, 0.0f, 1.0f);
    }
    if (aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_EMISSIVE, &ai_color) == AI_SUCCESS)
    {
        out_material_desc.emissive_color.r += ai_color.r;
        out_material_desc.emissive_color.g += ai_color.g;
        out_material_desc.emissive_color.b += ai_color.b;
        out_material_desc.emissive_color.a += ai_color.a;
        out_material_desc.emissive_color.a = Opal::Clamp(out_material_desc.emissive_color.a, 0.0f, 1.0f);
    }
    if (aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_DIFFUSE, &ai_color) == AI_SUCCESS)
    {
        out_material_desc.albedo_color = Vector4f(ai_color.r, ai_color.g, ai_color.b, ai_color.a);
        out_material_desc.albedo_color.a = Opal::Clamp(out_material_desc.albedo_color.a, 0.0f, 1.0f);
    }

    // Read opacity factor from the AI material and convert it to transparency factor. If opacity is 95% or more, the material is considered
    // opaque.
    constexpr float k_opaqueness_threshold = 0.05f;
    float opacity = 1.0f;
    if (aiGetMaterialFloat(ai_material, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS)
    {
        out_material_desc.transparency_factor = 1.0f - opacity;
        out_material_desc.transparency_factor = Opal::Clamp(out_material_desc.transparency_factor, 0.0f, 1.0f);
        if (out_material_desc.transparency_factor >= 1.0f - k_opaqueness_threshold)
        {
            out_material_desc.transparency_factor = 0.0f;
        }
    }

    // If AI material contains transparency factor as an RGB value, it will take precedence over the opacity factor.
    if (aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_TRANSPARENT, &ai_color) == AI_SUCCESS)
    {
        opacity = Opal::Max(Opal::Max(ai_color.r, ai_color.g), ai_color.b);
        out_material_desc.transparency_factor = Opal::Clamp(opacity, 0.0f, 1.0f);
        if (out_material_desc.transparency_factor >= 1.0f - k_opaqueness_threshold)
        {
            out_material_desc.transparency_factor = 0.0f;
        }
        out_material_desc.alpha_test = 0.5f;
    }

    // Read roughness and metallic factors from the AI material.
    float factor = 1.0f;
    if (aiGetMaterialFloat(ai_material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, &factor) == AI_SUCCESS)
    {
        out_material_desc.metallic_factor = factor;
    }
    if (aiGetMaterialFloat(ai_material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, &factor) == AI_SUCCESS)
    {
        out_material_desc.roughness = Vector4f(factor, factor, 0.0f, 0.0f);
    }

    // Get info about the texture file paths, store them in the out_texture_paths array and set the corresponding image ids in the material
    // description.
    aiString out_texture_path;
    aiTextureMapping out_texture_mapping = aiTextureMapping_UV;
    unsigned int out_uv_index = 0;
    float out_blend = 1.0f;
    aiTextureOp out_texture_op = aiTextureOp_Add;
    Opal::InPlaceArray<aiTextureMapMode, 2> out_texture_mode = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
    unsigned int out_texture_flags = 0;
    if (aiGetMaterialTexture(ai_material, aiTextureType_EMISSIVE, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.GetData(), &out_texture_flags) == AI_SUCCESS)
    {
        out_material_desc.emissive_texture_path = Opal::Paths::Combine(parent_path, out_texture_path.C_Str());
    }
    if (aiGetMaterialTexture(ai_material, aiTextureType_DIFFUSE, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.GetData(), &out_texture_flags) == AI_SUCCESS)
    {
        out_material_desc.albedo_texture_path = Opal::Paths::Combine(parent_path, out_texture_path.C_Str());
        // Some material heuristics
        const Opal::StringUtf8 albedo_map_path(out_texture_path.C_Str());
        if (Opal::Find(albedo_map_path, "grey_30") != Opal::StringUtf8::k_npos)
        {
            out_material_desc.material_flags |= MaterialFlags::Transparent;
        }
    }
    if (aiGetMaterialTexture(ai_material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &out_texture_path,
                             &out_texture_mapping, &out_uv_index, &out_blend, &out_texture_op, out_texture_mode.GetData(),
                             &out_texture_flags) == AI_SUCCESS)
    {
        out_material_desc.metallic_roughness_texture_path = Opal::Paths::Combine(parent_path, out_texture_path.C_Str());
    }
    if (aiGetMaterialTexture(ai_material, aiTextureType_LIGHTMAP, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.GetData(), &out_texture_flags) == AI_SUCCESS)
    {
        out_material_desc.ambient_occlusion_texture_path = Opal::Paths::Combine(parent_path, out_texture_path.C_Str());
    }
    if (aiGetMaterialTexture(ai_material, aiTextureType_NORMALS, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.GetData(), &out_texture_flags) == AI_SUCCESS)
    {
        out_material_desc.normal_texture_path = Opal::Paths::Combine(parent_path, out_texture_path.C_Str());
    }
    // In case that there is no normal map, try to read the height map that can be later converted into a normal map.
    if (out_material_desc.normal_texture_path.IsEmpty())
    {
        if (aiGetMaterialTexture(ai_material, aiTextureType_HEIGHT, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                                 &out_texture_op, out_texture_mode.GetData(), &out_texture_flags) == AI_SUCCESS)
        {
            out_material_desc.normal_texture_path = Opal::Paths::Combine(parent_path, out_texture_path.C_Str());
        }
    }
    if (aiGetMaterialTexture(ai_material, aiTextureType_OPACITY, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.GetData(), &out_texture_flags) == AI_SUCCESS)
    {
        // Opacity info will later be stored in the alpha channel of the albedo map.
        out_material_desc.opacity_texture_path = Opal::Paths::Combine(parent_path, out_texture_path.C_Str());
        out_material_desc.alpha_test = 0.5f;
    }

    // Material heuristics, modify material parameters based on the texture name so that it looks better.
    aiString ai_material_name;
    Opal::StringUtf8 material_name;
    if (aiGetMaterialString(ai_material, AI_MATKEY_NAME, &ai_material_name) == AI_SUCCESS)
    {
        material_name = ai_material_name.C_Str();
    }
    if ((Opal::Find(material_name, "Glass") != Opal::StringUtf8::k_npos) ||
        (Opal::Find(material_name, "Vespa_Headlight") != Opal::StringUtf8::k_npos))
    {
        out_material_desc.alpha_test = 0.75f;
        out_material_desc.transparency_factor = 0.1f;
        out_material_desc.material_flags |= MaterialFlags::Transparent;
    }
    else if (Opal::Find(material_name, "Bottle") != Opal::StringUtf8::k_npos)
    {
        out_material_desc.alpha_test = 0.54f;
        out_material_desc.transparency_factor = 0.4f;
        out_material_desc.material_flags |= MaterialFlags::Transparent;
    }
    else if (Opal::Find(material_name, "Metal") != Opal::StringUtf8::k_npos)
    {
        out_material_desc.metallic_factor = 1.0f;
        out_material_desc.roughness = Vector4f(0.1f, 0.1f, 0.0f, 0.0f);
    }
}
