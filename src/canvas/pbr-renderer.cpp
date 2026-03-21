#include "../../include/rndr/canvas/renderers/pbr-renderer.hpp"

#include "opal/paths.h"

#include "rndr/canvas/context.hpp"
#include "rndr/canvas/projections.hpp"
#include "rndr/log.hpp"
#include "rndr/mesh.hpp"

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

void Rndr::Canvas::PbrRenderer::DrawMesh(const Opal::StringUtf8& key, const Rndr::Mesh& mesh_data, const Matrix4x4f& transform,
                                         const PbrMaterialDesc& material)
{
    EnsureGeometry(key, Opal::AsBytes(mesh_data.vertices), Opal::AsBytes(mesh_data.indices));
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
    for (auto& batch : m_batches)
    {
        const BatchKey& batch_key = batch.key;
        BatchData& batch_data = batch.value;

        if (batch_data.instances.IsEmpty())
        {
            continue;
        }

        Brush& brush = batch_data.brush;

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

        auto geometry_it = m_geometry_cache.Find(batch_key.geometry_key);
        RNDR_ASSERT(geometry_it != m_geometry_cache.end(), "Geometry not found in cache!");
        Canvas::Mesh& mesh = geometry_it.GetValue();

        // Upload instance data to this batch's SSBO.
        batch_data.instance_buffer.Update(Opal::AsBytes(batch_data.instances));
        brush.SetBuffer("instances", batch_data.instance_buffer);

        draw_list.DrawInstanced(mesh, brush, static_cast<u32>(batch_data.instances.GetSize()));
    }
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
