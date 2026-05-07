#include "rndr/canvas/renderers/pbr-renderer.hpp"

#include "assimp/cimport.h"
#include "assimp/GltfMaterial.h"
#include "assimp/material.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "opal/container/in-place-array.h"
#include "opal/exceptions.h"
#include "opal/math-base.h"
#include "opal/paths.h"

#include "rndr/canvas/context.hpp"
#include "rndr/log.hpp"

// BatchKey ==================================================================

bool Rndr::Canvas::PbrRenderer::BatchKey::operator==(const BatchKey& other) const
{
    if (geometry_key != other.geometry_key)
    {
        return false;
    }
    for (int i = 0; i < textures.GetSize(); ++i)
    {
        if (textures[i] != other.textures[i])
        {
            return false;
        }
    }
    return true;
}

Opal::u64 Opal::Hasher<Rndr::Canvas::PbrRenderer::BatchKey>::operator()(const Rndr::Canvas::PbrRenderer::BatchKey& key) const
{
    u64 hash = Hasher<StringUtf8>()(key.geometry_key);
    for (const auto& tex : key.textures)
    {
        hash ^= Hasher<u64>()(reinterpret_cast<u64>(tex.GetPtr())) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    }
    return hash;
}

// PbrRenderer ===============================================================

Rndr::Canvas::PbrRenderer::PbrRenderer(Opal::Ref<Context> context) : m_context(std::move(context))
{
    const Opal::StringUtf8 shader_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "shaders", "canvas-pbr.slang");
    m_shader = Shader::FromSource(shader_path, "PBR Renderer");
    RNDR_ASSERT(m_shader.IsValid(), "Failed to create PbrRenderer shader!");

    // 1x1 white dummy texture for unused texture slots.
    m_dummy_texture =
        Texture(*m_context, TextureDesc{.width = 1, .height = 1}, Opal::AsBytes(Colors::k_white), "PBR Renderer - Dummy Texture");
}

Rndr::Canvas::PbrRenderer::~PbrRenderer()
{
    Destroy();
}

void Rndr::Canvas::PbrRenderer::Destroy()
{
    m_batches.Clear();
    m_geometry_cache.Clear();
    m_dummy_texture.Destroy();
    m_shader.Destroy();
}

void Rndr::Canvas::PbrRenderer::BeginFrame()
{
    for (auto& batch : m_batches)
    {
        batch.value.instances.Clear();
    }
    m_directional_lights.Clear();
    m_point_lights.Clear();
}

void Rndr::Canvas::PbrRenderer::SetViewProjection(const Matrix4x4f& view_projection)
{
    m_view_projection = view_projection;
}

void Rndr::Canvas::PbrRenderer::SetCameraPosition(const Point3f& camera_position)
{
    m_camera_position = camera_position;
}

void Rndr::Canvas::PbrRenderer::AddDirectionalLight(const Vector3f& direction, const Vector4f& color)
{
    m_directional_lights.PushBack({.direction = direction, .color = color});
}

void Rndr::Canvas::PbrRenderer::AddPointLight(const Point3f& position, const Vector4f& color)
{
    m_point_lights.PushBack({.position = position, .color = color});
}

// Geometry ------------------------------------------------------------------

void Rndr::Canvas::PbrRenderer::EnsureGeometry(const Opal::StringUtf8& key, const Opal::ArrayView<const u8>& vertex_data,
                                               const Opal::ArrayView<const u8>& index_data)
{
    if (m_geometry_cache.Contains(key))
    {
        return;
    }
    const VertexLayout vertex_layout = m_shader.GetVertexLayout().Clone();
    Canvas::Mesh mesh(vertex_layout, vertex_data, index_data, key.Clone());
    RNDR_ASSERT(mesh.IsValid(), "Failed to create PbrRenderer mesh!");
    m_geometry_cache.Insert(key.Clone(), std::move(mesh));
}

void Rndr::Canvas::PbrRenderer::DrawCube(const Matrix4x4f& transform, const PbrMaterialDesc& material, f32 u_tiling, f32 v_tiling)
{
    Opal::StringUtf8 key(64, '\0');
    snprintf(key.GetData(), key.GetSize(), "Cube_%u_%u", static_cast<u32>(u_tiling), static_cast<u32>(v_tiling));
    key.Trim();

    if (!m_geometry_cache.Contains(key))
    {
        Opal::DynamicArray<u8> vertex_data;
        Opal::DynamicArray<u8> index_data;
        GenerateCube(vertex_data, index_data, u_tiling, v_tiling);
        EnsureGeometry(key, Opal::AsBytes(vertex_data), Opal::AsBytes(index_data));
    }

    AddDrawEntry(key, transform, material);
}

void Rndr::Canvas::PbrRenderer::DrawSphere(const Matrix4x4f& transform, const PbrMaterialDesc& material, f32 u_tiling, f32 v_tiling,
                                           u32 latitude_segments, u32 longitude_segments)
{
    Opal::StringUtf8 key(128, '\0');
    snprintf(key.GetData(), key.GetSize(), "Sphere_%u_%u_%u_%u", latitude_segments, longitude_segments, static_cast<u32>(u_tiling),
             static_cast<u32>(v_tiling));
    key.Trim();

    if (!m_geometry_cache.Contains(key))
    {
        Opal::DynamicArray<u8> vertex_data;
        Opal::DynamicArray<u8> index_data;
        GenerateSphere(vertex_data, index_data, latitude_segments, longitude_segments, u_tiling, v_tiling);
        EnsureGeometry(key, Opal::AsBytes(vertex_data), Opal::AsBytes(index_data));
    }

    AddDrawEntry(key, transform, material);
}

void Rndr::Canvas::PbrRenderer::DrawMesh(const Opal::StringUtf8& key, const Mesh& mesh, const Matrix4x4f& transform,
                                         const PbrMaterialDesc& material)
{
    if (!m_external_geometry.Contains(key))
    {
        m_external_geometry.Insert(key.Clone(), Opal::Ref<const Mesh>(mesh));
    }
    AddDrawEntry(key, transform, material);
}

// Draw entry recording ------------------------------------------------------

Rndr::u32 Rndr::Canvas::PbrRenderer::ComputeMaterialFlags(const PbrMaterialDesc& material)
{
    u32 flags = 0;
    if (material.albedo_texture != nullptr)
    {
        flags |= k_flag_albedo_texture;
    }
    if (material.emissive_texture != nullptr)
    {
        flags |= k_flag_emissive_texture;
    }
    if (material.metallic_roughness_texture != nullptr)
    {
        flags |= k_flag_metallic_roughness_texture;
    }
    if (material.normal_texture != nullptr)
    {
        flags |= k_flag_normal_texture;
    }
    if (material.ambient_occlusion_texture != nullptr)
    {
        flags |= k_flag_ambient_occlusion_texture;
    }
    if (material.opacity_texture != nullptr)
    {
        flags |= k_flag_opacity_texture;
    }
    return flags;
}

Rndr::Canvas::PbrRenderer::InstanceData Rndr::Canvas::PbrRenderer::MakeInstanceData(const Matrix4x4f& transform,
                                                                                    const PbrMaterialDesc& material)
{
    InstanceData data;
    data.model_transform = transform;
    data.normal_transform = Opal::Transpose(Opal::Inverse(transform));
    data.albedo_color = material.albedo_color;
    data.emissive_color = material.emissive_color;
    data.roughness = material.roughness;
    data.metallic_factor = material.metallic_factor;
    data.transparency_factor = material.transparency_factor;
    data.alpha_test = material.alpha_test;
    data.material_flags = ComputeMaterialFlags(material);
    return data;
}

void Rndr::Canvas::PbrRenderer::AddDrawEntry(const Opal::StringUtf8& geometry_key, const Matrix4x4f& transform,
                                             const PbrMaterialDesc& material)
{
    BatchKey batch_key;
    batch_key.geometry_key = geometry_key.Clone();
    batch_key.textures[0] = material.albedo_texture.Clone();
    batch_key.textures[1] = material.emissive_texture.Clone();
    batch_key.textures[2] = material.metallic_roughness_texture.Clone();
    batch_key.textures[3] = material.normal_texture.Clone();
    batch_key.textures[4] = material.ambient_occlusion_texture.Clone();
    batch_key.textures[5] = material.opacity_texture.Clone();

    auto it = m_batches.Find(batch_key);
    if (it == m_batches.end())
    {
        BatchData data;
        data.brush = Brush(BrushDesc{.depth_test = true, .depth_write = true}, "PBR Renderer - " + material.material_name.Clone());
        data.brush.SetShader(m_shader);
        data.instance_buffer = Buffer(BufferUsage::Storage, k_max_instance_count * sizeof(InstanceData), 0, {},
                                      "PBR Renderer - " + material.material_name.Clone() + " - Instance Buffer");
        BindTextures(data.brush, batch_key);
        m_batches.Insert(batch_key.Clone(), std::move(data));
        it = m_batches.Find(batch_key);
    }
    it.GetValue().instances.PushBack(MakeInstanceData(transform, material));
}

// Rendering -----------------------------------------------------------------

void Rndr::Canvas::PbrRenderer::BindTextures(Brush& brush, const BatchKey& key)
{
    static constexpr const char* k_texture_names[] = {
        "albedo_texture", "emissive_texture",          "metallic_roughness_texture",
        "normal_texture", "ambient_occlusion_texture", "opacity_texture",
    };
    for (i32 i = 0; i < 6; ++i)
    {
        brush.SetTexture(k_texture_names[i], key.textures[i] != nullptr ? *key.textures[i] : m_dummy_texture);
    }
}

void Rndr::Canvas::PbrRenderer::Render(DrawList& draw_list)
{
    draw_list.BeginEvent("PbrRenderer::Render");
    for (auto& batch : m_batches)
    {
        const BatchKey& batch_key = batch.key;
        BatchData& batch_data = batch.value;

        if (batch_data.instances.IsEmpty())
        {
            continue;
        }

        Brush& brush = batch_data.brush;

        brush.SetUniform("draw_flags", m_draw_flags);

        // Set per-frame uniforms on this batch's brush.
        brush.SetUniform("view_projection", m_view_projection);
        brush.SetUniform("camera_position", m_camera_position);

        const u32 dir_count = Opal::Min(static_cast<u32>(m_directional_lights.GetSize()), k_max_light_count);
        brush.SetUniform("directional_light_count", dir_count);
        for (u32 i = 0; i < dir_count; ++i)
        {
            const Vector4f dir = {m_directional_lights[i].direction.x, m_directional_lights[i].direction.y,
                                  m_directional_lights[i].direction.z, 0.0f};
            brush.SetUniform("directional_light_directions", static_cast<i32>(i), dir);
            brush.SetUniform("directional_light_colors", static_cast<i32>(i), m_directional_lights[i].color);
        }

        const u32 point_count = Opal::Min(static_cast<u32>(m_point_lights.GetSize()), k_max_light_count);
        brush.SetUniform("point_light_count", point_count);
        for (u32 i = 0; i < point_count; ++i)
        {
            const Vector4f pos = {m_point_lights[i].position.x, m_point_lights[i].position.y, m_point_lights[i].position.z, 0.0f};
            brush.SetUniform("point_light_positions", static_cast<i32>(i), pos);
            brush.SetUniform("point_light_colors", static_cast<i32>(i), m_point_lights[i].color);
        }

        Canvas::Mesh* mesh = nullptr;
        if (auto external_it = m_external_geometry.Find(batch_key.geometry_key); external_it != m_external_geometry.end())
        {
            mesh = const_cast<Canvas::Mesh*>(external_it.GetValue().GetPtr());
        }
        else if (auto owned_it = m_geometry_cache.Find(batch_key.geometry_key); owned_it != m_geometry_cache.end())
        {
            mesh = &owned_it.GetValue();
        }
        RNDR_ASSERT(mesh != nullptr, "Geometry not found in cache!");

        // Upload instance data to this batch's SSBO.
        batch_data.instance_buffer.Update(Opal::AsBytes(batch_data.instances));
        brush.SetBuffer("instances", batch_data.instance_buffer);

        draw_list.DrawInstanced(*mesh, brush, static_cast<u32>(batch_data.instances.GetSize()));
    }
    draw_list.EndEvent("PbrRenderer::Render");
}

// Geometry generators -------------------------------------------------------

void Rndr::Canvas::PbrRenderer::GenerateCube(Opal::DynamicArray<u8>& out_vertex_data, Opal::DynamicArray<u8>& out_index_data, f32 u_tiling,
                                             f32 v_tiling)
{
    struct Vertex
    {
        Point3f position;
        Vector3f normal;
        Vector2f tex_coord;
    };

    const f32 h = 0.5f;

    struct FaceData
    {
        Vector3f normal;
        Point3f verts[4];
        Vector2f uvs[4];
    };

    // clang-format off
    const FaceData faces[6] = {
        // Front (Z+)
        {{0, 0, 1}, {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}},
         {{0, 0}, {u_tiling, 0}, {u_tiling, v_tiling}, {0, v_tiling}}},
        // Back (Z-)
        {{0, 0, -1}, {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}},
         {{0, 0}, {u_tiling, 0}, {u_tiling, v_tiling}, {0, v_tiling}}},
        // Top (Y+)
        {{0, 1, 0}, {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}},
         {{0, 0}, {u_tiling, 0}, {u_tiling, v_tiling}, {0, v_tiling}}},
        // Bottom (Y-)
        {{0, -1, 0}, {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}},
         {{0, 0}, {u_tiling, 0}, {u_tiling, v_tiling}, {0, v_tiling}}},
        // Right (X+)
        {{1, 0, 0}, {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}},
         {{0, 0}, {u_tiling, 0}, {u_tiling, v_tiling}, {0, v_tiling}}},
        // Left (X-)
        {{-1, 0, 0}, {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}},
         {{0, 0}, {u_tiling, 0}, {u_tiling, v_tiling}, {0, v_tiling}}},
    };
    // clang-format on

    for (u32 f = 0; f < 6; ++f)
    {
        for (u32 v = 0; v < 4; ++v)
        {
            Vertex vertex;
            vertex.position = faces[f].verts[v];
            vertex.normal = faces[f].normal;
            vertex.tex_coord = faces[f].uvs[v];
            out_vertex_data.Append(Opal::AsWritableBytes(vertex));
        }

        u32 base = f * 4;
        u32 idx0 = base + 0;
        u32 idx1 = base + 1;
        u32 idx2 = base + 2;
        u32 idx3 = base + 3;
        out_index_data.Append(Opal::AsWritableBytes(idx0));
        out_index_data.Append(Opal::AsWritableBytes(idx1));
        out_index_data.Append(Opal::AsWritableBytes(idx2));
        out_index_data.Append(Opal::AsWritableBytes(idx0));
        out_index_data.Append(Opal::AsWritableBytes(idx2));
        out_index_data.Append(Opal::AsWritableBytes(idx3));
    }
}

void Rndr::Canvas::PbrRenderer::GenerateSphere(Opal::DynamicArray<u8>& out_vertex_data, Opal::DynamicArray<u8>& out_index_data,
                                               u32 latitude_segments, u32 longitude_segments, f32 u_tiling, f32 v_tiling)
{
    struct Vertex
    {
        Point3f position;
        Vector3f normal;
        Vector2f tex_coord;
    };

    for (u32 lat = 0; lat <= latitude_segments; ++lat)
    {
        const f32 theta = static_cast<f32>(lat) * Opal::k_pi_float / static_cast<f32>(latitude_segments);
        const f32 sin_theta = Opal::Sin(theta);
        const f32 cos_theta = Opal::Cos(theta);

        for (u32 lon = 0; lon <= longitude_segments; ++lon)
        {
            const f32 phi = static_cast<f32>(lon) * 2.0f * Opal::k_pi_float / static_cast<f32>(longitude_segments);
            const f32 sin_phi = Opal::Sin(phi);
            const f32 cos_phi = Opal::Cos(phi);

            const f32 x = cos_phi * sin_theta;
            const f32 y = cos_theta;
            const f32 z = sin_phi * sin_theta;

            const f32 u = (static_cast<f32>(lon) / static_cast<f32>(longitude_segments)) * u_tiling;
            const f32 v = (static_cast<f32>(lat) / static_cast<f32>(latitude_segments)) * v_tiling;

            Vertex vertex;
            vertex.position = {x, y, z};
            vertex.normal = {x, y, z};
            vertex.tex_coord = {u, v};
            out_vertex_data.Append(Opal::AsWritableBytes(vertex));
        }
    }

    for (u32 lat = 0; lat < latitude_segments; ++lat)
    {
        for (u32 lon = 0; lon < longitude_segments; ++lon)
        {
            u32 current = lat * (longitude_segments + 1) + lon;
            u32 inc_current = current + 1;
            u32 next = current + longitude_segments + 1;
            u32 inc_next = next + 1;
            out_index_data.Append(Opal::AsWritableBytes(current));
            out_index_data.Append(Opal::AsWritableBytes(inc_current));
            out_index_data.Append(Opal::AsWritableBytes(next));
            out_index_data.Append(Opal::AsWritableBytes(inc_current));
            out_index_data.Append(Opal::AsWritableBytes(inc_next));
            out_index_data.Append(Opal::AsWritableBytes(next));
        }
    }
}

// Model loading -------------------------------------------------------------

namespace
{

void ExtractMeshDataFromScene(const aiScene& ai_scene, Opal::DynamicArray<Rndr::u8>& out_vertex_data,
                              Opal::DynamicArray<Rndr::u8>& out_index_data)
{
    if (!ai_scene.HasMeshes())
    {
        throw Opal::Exception("No meshes found!");
    }

    const aiMesh* ai_mesh = ai_scene.mMeshes[0];
    for (Rndr::u32 vertex_idx = 0; vertex_idx < ai_mesh->mNumVertices; ++vertex_idx)
    {
        Rndr::Point3f position(ai_mesh->mVertices[vertex_idx].x, ai_mesh->mVertices[vertex_idx].y, ai_mesh->mVertices[vertex_idx].z);
        Rndr::Normal3f normal(ai_mesh->mNormals[vertex_idx].x, ai_mesh->mNormals[vertex_idx].y, ai_mesh->mNormals[vertex_idx].z);
        Rndr::Point2f uv(ai_mesh->mTextureCoords[0][vertex_idx].x, ai_mesh->mTextureCoords[0][vertex_idx].y);
        out_vertex_data.Append(Opal::AsWritableBytes(position));
        out_vertex_data.Append(Opal::AsWritableBytes(normal));
        out_vertex_data.Append(Opal::AsWritableBytes(uv));
    }

    for (Rndr::u32 face_idx = 0; face_idx < ai_mesh->mNumFaces; ++face_idx)
    {
        const aiFace& face = ai_mesh->mFaces[face_idx];
        if (face.mNumIndices != 3)
        {
            continue;
        }
        for (Rndr::u32 index_idx = 0; index_idx < face.mNumIndices; ++index_idx)
        {
            out_index_data.Append(Opal::AsWritableBytes<Rndr::u32>(face.mIndices[index_idx]));
        }
    }
}

Opal::StringUtf8 GetTexturePath(const aiMaterial* ai_material, aiTextureType type, unsigned int index, const Opal::StringUtf8& parent_path)
{
    aiString out_texture_path;
    aiTextureMapping out_texture_mapping = aiTextureMapping_UV;
    unsigned int out_uv_index = 0;
    float out_blend = 1.0f;
    aiTextureOp out_texture_op = aiTextureOp_Add;
    Opal::InPlaceArray<aiTextureMapMode, 2> out_texture_mode = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
    unsigned int out_texture_flags = 0;
    if (aiGetMaterialTexture(ai_material, type, index, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend, &out_texture_op,
                             out_texture_mode.GetData(), &out_texture_flags) == AI_SUCCESS)
    {
        return Opal::Paths::NormalizePath(Opal::Paths::Combine(parent_path, out_texture_path.C_Str()));
    }
    return {};
}

void LoadMaterialFromScene(const aiScene& ai_scene, Rndr::u32 material_index, const Opal::StringUtf8& parent_path,
                           const Rndr::Canvas::Context& context, const Rndr::Canvas::TextureDesc& texture_desc, bool flip_vertically,
                           Rndr::Canvas::PbrModel& out_model)
{
    const aiMaterial* ai_material = ai_scene.mMaterials[material_index];

    aiColor4D ai_color;
    if (aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_AMBIENT, &ai_color) == AI_SUCCESS)
    {
        out_model.emissive_color = Rndr::Vector4f(ai_color.r, ai_color.g, ai_color.b, ai_color.a);
        out_model.emissive_color.a = Opal::Clamp(out_model.emissive_color.a, 0.0f, 1.0f);
    }
    if (aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_EMISSIVE, &ai_color) == AI_SUCCESS)
    {
        out_model.emissive_color.r += ai_color.r;
        out_model.emissive_color.g += ai_color.g;
        out_model.emissive_color.b += ai_color.b;
        out_model.emissive_color.a += ai_color.a;
        out_model.emissive_color.a = Opal::Clamp(out_model.emissive_color.a, 0.0f, 1.0f);
    }
    if (aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_DIFFUSE, &ai_color) == AI_SUCCESS)
    {
        out_model.albedo_color = Rndr::Vector4f(ai_color.r, ai_color.g, ai_color.b, ai_color.a);
        out_model.albedo_color.a = Opal::Clamp(out_model.albedo_color.a, 0.0f, 1.0f);
    }

    constexpr float k_opaqueness_threshold = 0.05f;
    float opacity = 1.0f;
    if (aiGetMaterialFloat(ai_material, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS)
    {
        out_model.transparency_factor = 1.0f - opacity;
        out_model.transparency_factor = Opal::Clamp(out_model.transparency_factor, 0.0f, 1.0f);
        if (out_model.transparency_factor >= 1.0f - k_opaqueness_threshold)
        {
            out_model.transparency_factor = 0.0f;
        }
    }

    if (aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_TRANSPARENT, &ai_color) == AI_SUCCESS)
    {
        opacity = Opal::Max(Opal::Max(ai_color.r, ai_color.g), ai_color.b);
        out_model.transparency_factor = Opal::Clamp(opacity, 0.0f, 1.0f);
        if (out_model.transparency_factor >= 1.0f - k_opaqueness_threshold)
        {
            out_model.transparency_factor = 0.0f;
        }
        out_model.alpha_test = 0.5f;
    }

    float factor = 1.0f;
    if (aiGetMaterialFloat(ai_material, AI_MATKEY_METALLIC_FACTOR, &factor) == AI_SUCCESS)
    {
        out_model.metallic_factor = factor;
    }
    if (aiGetMaterialFloat(ai_material, AI_MATKEY_ROUGHNESS_FACTOR, &factor) == AI_SUCCESS)
    {
        out_model.roughness = Rndr::Vector4f(factor, factor, 0.0f, 0.0f);
    }

    // Load textures.
    Opal::StringUtf8 path;

    path = GetTexturePath(ai_material, aiTextureType_EMISSIVE, 0, parent_path);
    if (!path.IsEmpty())
    {
        out_model.emissive_texture = Rndr::Canvas::Texture::FromFile(context, path, texture_desc, flip_vertically);
    }

    path = GetTexturePath(ai_material, aiTextureType_DIFFUSE, 0, parent_path);
    if (!path.IsEmpty())
    {
        out_model.albedo_texture = Rndr::Canvas::Texture::FromFile(context, path, texture_desc, flip_vertically);
    }

    aiString mr_path;
    aiTextureMapping mr_mapping = aiTextureMapping_UV;
    unsigned int mr_uv = 0;
    float mr_blend = 1.0f;
    aiTextureOp mr_op = aiTextureOp_Add;
    Opal::InPlaceArray<aiTextureMapMode, 2> mr_mode = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
    unsigned int mr_flags = 0;
    if (aiGetMaterialTexture(ai_material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &mr_path, &mr_mapping, &mr_uv,
                             &mr_blend, &mr_op, mr_mode.GetData(), &mr_flags) == AI_SUCCESS)
    {
        Opal::StringUtf8 mr_full_path = Opal::Paths::NormalizePath(Opal::Paths::Combine(parent_path, mr_path.C_Str()));
        out_model.metallic_roughness_texture = Rndr::Canvas::Texture::FromFile(context, mr_full_path, texture_desc, flip_vertically);
    }

    path = GetTexturePath(ai_material, aiTextureType_LIGHTMAP, 0, parent_path);
    if (!path.IsEmpty())
    {
        out_model.ambient_occlusion_texture = Rndr::Canvas::Texture::FromFile(context, path, texture_desc, flip_vertically);
    }

    path = GetTexturePath(ai_material, aiTextureType_NORMALS, 0, parent_path);
    if (path.IsEmpty())
    {
        path = GetTexturePath(ai_material, aiTextureType_HEIGHT, 0, parent_path);
    }
    if (!path.IsEmpty())
    {
        out_model.normal_texture = Rndr::Canvas::Texture::FromFile(context, path, texture_desc, flip_vertically);
    }

    path = GetTexturePath(ai_material, aiTextureType_OPACITY, 0, parent_path);
    if (!path.IsEmpty())
    {
        out_model.opacity_texture = Rndr::Canvas::Texture::FromFile(context, path, texture_desc, flip_vertically);
        out_model.alpha_test = 0.5f;
    }

    // Material name.
    aiString ai_material_name;
    if (aiGetMaterialString(ai_material, AI_MATKEY_NAME, &ai_material_name) == AI_SUCCESS)
    {
        out_model.material_name = ai_material_name.C_Str();
    }

    // Material heuristics.
    if ((Opal::Find(out_model.material_name, "Glass") != Opal::StringUtf8::k_npos) ||
        (Opal::Find(out_model.material_name, "Vespa_Headlight") != Opal::StringUtf8::k_npos))
    {
        out_model.alpha_test = 0.75f;
        out_model.transparency_factor = 0.1f;
    }
    else if (Opal::Find(out_model.material_name, "Bottle") != Opal::StringUtf8::k_npos)
    {
        out_model.alpha_test = 0.54f;
        out_model.transparency_factor = 0.4f;
    }
    else if (Opal::Find(out_model.material_name, "Metal") != Opal::StringUtf8::k_npos)
    {
        out_model.metallic_factor = 1.0f;
        out_model.roughness = Rndr::Vector4f(0.1f, 0.1f, 0.0f, 0.0f);
    }
}

}  // namespace

Rndr::Canvas::PbrModel Rndr::Canvas::PbrRenderer::LoadModel(const Opal::StringUtf8& file_path, const TextureDesc& texture_desc,
                                                             bool flip_vertically)
{
    constexpr u32 k_ai_process_flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                       aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                                       aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                       aiProcess_GenUVCoords;

    const aiScene* scene = aiImportFile(*file_path, k_ai_process_flags);
    if (scene == nullptr)
    {
        throw Opal::Exception("Failed to load model");
    }

    PbrModel model;

    const Opal::StringUtf8 mesh_name = Opal::Paths::GetFileName(file_path).GetValue();
    Opal::DynamicArray<u8> vertex_data;
    Opal::DynamicArray<u8> index_data;
    ExtractMeshDataFromScene(*scene, vertex_data, index_data);
    const VertexLayout vertex_layout = m_shader.GetVertexLayout().Clone();
    model.mesh = Mesh(vertex_layout, Opal::AsBytes(vertex_data), Opal::AsBytes(index_data), mesh_name.Clone());

    if (scene->HasMeshes() && scene->mMeshes[0]->mMaterialIndex < scene->mNumMaterials)
    {
        const Opal::StringUtf8 parent_path = Opal::Paths::GetParentPath(file_path).GetValue();
        LoadMaterialFromScene(*scene, scene->mMeshes[0]->mMaterialIndex, parent_path, *m_context, texture_desc, flip_vertically, model);
    }

    aiReleaseImport(scene);
    return model;
}

void Rndr::Canvas::PbrRenderer::DrawModel(const Opal::StringUtf8& key, const PbrModel& model, const Matrix4x4f& transform)
{
    PbrMaterialDesc desc;
    desc.material_name = model.material_name.Clone();
    desc.albedo_color = model.albedo_color;
    desc.emissive_color = model.emissive_color;
    desc.roughness = model.roughness;
    desc.metallic_factor = model.metallic_factor;
    desc.transparency_factor = model.transparency_factor;
    desc.alpha_test = model.alpha_test;
    if (model.albedo_texture.IsValid())
    {
        desc.albedo_texture = Opal::Ref<const Texture>(model.albedo_texture);
    }
    if (model.emissive_texture.IsValid())
    {
        desc.emissive_texture = Opal::Ref<const Texture>(model.emissive_texture);
    }
    if (model.metallic_roughness_texture.IsValid())
    {
        desc.metallic_roughness_texture = Opal::Ref<const Texture>(model.metallic_roughness_texture);
    }
    if (model.normal_texture.IsValid())
    {
        desc.normal_texture = Opal::Ref<const Texture>(model.normal_texture);
    }
    if (model.ambient_occlusion_texture.IsValid())
    {
        desc.ambient_occlusion_texture = Opal::Ref<const Texture>(model.ambient_occlusion_texture);
    }
    if (model.opacity_texture.IsValid())
    {
        desc.opacity_texture = Opal::Ref<const Texture>(model.opacity_texture);
    }
    DrawMesh(key, model.mesh, transform, desc);
}
