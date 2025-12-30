#include "rndr/renderers/shape-3d-renderer.hpp"

#include "assimp/Vertex.h"
#include "opal/paths.h"

#include "rndr/file.hpp"
#include "rndr/input-layout-builder.hpp"
#include "rndr/trace.hpp"

namespace
{
OPAL_START_DISABLE_WARNINGS
OPAL_DISABLE_MSVC_WARNING(4324)
struct alignas(16) PerFrameData
{
    constexpr static Rndr::u32 k_max_light_count = 4;
    Rndr::Matrix4x4f view_projection_transform;
    Rndr::Point3f camera_position_world;
    Rndr::u32 directional_light_count;
    Rndr::Vector4f direction_light_world[k_max_light_count];
    Rndr::Vector4f direction_light_color[k_max_light_count];
    Rndr::Point4f point_light_world[k_max_light_count];
    Rndr::Vector4f point_light_color[k_max_light_count];
    Rndr::u32 point_light_count;
};
OPAL_END_DISABLE_WARNINGS
}  // namespace

Rndr::Shape3DRenderer::Shape3DRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc, Opal::Ref<FrameBuffer> target)
    : RendererBase(name, desc), m_target(target)
{
    const Opal::StringUtf8 shader_dir = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "shaders");
    const Opal::StringUtf8 vertex_shader_contents = File::ReadShader(shader_dir, "pbr.vert");
    m_fragment_shader_source = File::ReadShader(shader_dir, "pbr.frag");

    m_vertex_shader =
        Shader(m_desc.graphics_context,
               {.type = ShaderType::Vertex, .source = vertex_shader_contents, .debug_name = "Shape 3D Renderer - Vertex Shader"});
    RNDR_ASSERT(m_vertex_shader.IsValid(), "Failed to create vertex shader!");

    m_vertex_buffer = Buffer(desc.graphics_context, {.type = BufferType::ShaderStorage,
                                                     .usage = Usage::Dynamic,
                                                     .size = k_max_vertex_count * sizeof(VertexData),
                                                     .stride = sizeof(VertexData),
                                                     .debug_name = "Shape 3D Renderer - Vertex Buffer"});
    RNDR_ASSERT(m_vertex_buffer.IsValid(), "Failed to create vertex buffer!");
    m_index_buffer = Buffer(desc.graphics_context, {.type = BufferType::Index,
                                                    .usage = Usage::Dynamic,
                                                    .size = k_max_index_count * sizeof(uint32_t),
                                                    .stride = sizeof(uint32_t),
                                                    .debug_name = "Shape 3D Renderer - Index Buffer"});
    RNDR_ASSERT(m_index_buffer.IsValid(), "Failed to create index buffer!");
    m_model_transform_buffer = Buffer(desc.graphics_context, {.type = BufferType::ShaderStorage,
                                                              .usage = Usage::Dynamic,
                                                              .size = k_max_instance_count * sizeof(InstanceData),
                                                              .stride = sizeof(InstanceData),
                                                              .debug_name = "Shape 3D Renderer - Instance Transforms Buffer"});
    RNDR_ASSERT(m_model_transform_buffer.IsValid(), "Failed to create instance buffer!");
    m_per_frame_buffer = Buffer(m_desc.graphics_context, {.type = BufferType::Constant,
                                                          .usage = Usage::Dynamic,
                                                          .size = sizeof(PerFrameData),
                                                          .stride = sizeof(PerFrameData),
                                                          .debug_name = "Shape 3D Renderer - Per Frame Buffer"});
    RNDR_ASSERT(m_per_frame_buffer.IsValid(), "Failed to create per-frame-data buffer!");
    m_draw_commands_buffer = Buffer(m_desc.graphics_context, {.type = BufferType::DrawCommands,
                                                              .usage = Usage::Dynamic,
                                                              .size = k_max_instance_count * sizeof(DrawIndicesData),
                                                              .stride = sizeof(DrawIndicesData),
                                                              .debug_name = "Shape 3D Renderer - Draw Commands Buffer"});
    RNDR_ASSERT(m_draw_commands_buffer.IsValid(), "Failed to create draw commands buffer!");

    const InputLayoutDesc input_layout_desc = Rndr::InputLayoutBuilder()
                                                  .AddShaderStorage(m_vertex_buffer, 1)
                                                  .AddShaderStorage(m_model_transform_buffer, 2)
                                                  .AddIndexBuffer(m_index_buffer)
                                                  .Build();
}

Rndr::Shape3DRenderer::~Shape3DRenderer()
{
    Destroy();
}

void Rndr::Shape3DRenderer::Destroy()
{
    for (auto& perm : m_shader_permutations)
    {
        perm.Destroy();
    }
    m_shader_permutations.Clear();
    for (auto& pair : m_pipelines)
    {
        pair.value.Destroy();
    }
    m_pipelines.Clear();
    m_per_frame_buffer.Destroy();
    m_model_transform_buffer.Destroy();
    m_index_buffer.Destroy();
    m_vertex_buffer.Destroy();
    m_draw_commands_buffer.Destroy();
    m_vertex_shader.Destroy();
}

void Rndr::Shape3DRenderer::SetTransforms(const Matrix4x4f& view, const Matrix4x4f& projection)
{
    m_view = view;
    m_projection = projection;
}
void Rndr::Shape3DRenderer::SetCameraPosition(const Point3f& camera_position)
{
    m_camera_position = camera_position;
}

bool Rndr::Shape3DRenderer::Render(f32 delta_seconds, CommandList& command_list)
{
    RNDR_UNUSED(delta_seconds);

    command_list.CmdPushMarker("Shape 3D Render");

    if (m_is_geometry_data_dirty)
    {
        m_is_geometry_data_dirty = false;
        command_list.CmdUpdateBuffer(m_vertex_buffer, Opal::AsBytes(m_vertex_data));
        command_list.CmdUpdateBuffer(m_index_buffer, Opal::AsBytes(m_index_data));
    }

    if (m_directional_lights.IsEmpty() && m_point_lights.IsEmpty())
    {
        throw Opal::Exception("No lights provided!");
    }

    PerFrameData per_frame_data;
    per_frame_data.view_projection_transform = Opal::Transpose(m_projection * m_view);
    per_frame_data.camera_position_world = m_camera_position;
    for (u32 light_idx = 0; light_idx < PerFrameData::k_max_light_count && light_idx < m_directional_lights.GetSize(); ++light_idx)
    {
        const DirectionalLight& dl = m_directional_lights[light_idx];
        per_frame_data.direction_light_world[light_idx] = Vector4f(dl.direction.x, dl.direction.y, dl.direction.z, 0.0f);
        per_frame_data.direction_light_color[light_idx] = dl.color;
    }
    per_frame_data.directional_light_count = Opal::Min(static_cast<u32>(m_directional_lights.GetSize()), PerFrameData::k_max_light_count);
    for (u32 light_idx = 0; light_idx < PerFrameData::k_max_light_count && light_idx < m_point_lights.GetSize(); ++light_idx)
    {
        const PointLight& pl = m_point_lights[light_idx];
        per_frame_data.point_light_world[light_idx] = Point4f(pl.position.x, pl.position.y, pl.position.z, 0.0f);
        per_frame_data.point_light_color[light_idx] = pl.color;
    }
    per_frame_data.point_light_count = Opal::Min(static_cast<u32>(m_point_lights.GetSize()), PerFrameData::k_max_light_count);
    command_list.CmdUpdateBuffer(m_per_frame_buffer, Opal::AsBytes(per_frame_data));

    if (m_target.IsValid())
    {
        command_list.CmdBindFrameBuffer(*m_target);
    }
    else
    {
        command_list.CmdBindSwapChainFrameBuffer(m_desc.swap_chain);
    }

    for (auto& pair : m_materials)
    {
        command_list.CmdPushMarker("PBR Model Rendering - Material");
        const Material* material = pair.key.material.GetPtr();
        PerMaterialData& material_data = pair.value;
        const Pipeline& pipeline = m_pipelines.GetValue(pair.key);
        command_list.CmdBindPipeline(pipeline);
        command_list.CmdBindBuffer(m_per_frame_buffer, 0);
        command_list.CmdUpdateBuffer(m_model_transform_buffer, Opal::AsBytes(material_data.instances));
        material->BindResources(command_list);
        command_list.CmdDrawIndicesMulti(Opal::Ref{m_draw_commands_buffer}, PrimitiveTopology::Triangle,
                                         Opal::ArrayView<DrawIndicesData>(material_data.draw_commands));
        command_list.CmdPopMarker();
    }
    m_materials.Clear();
    m_directional_lights.Clear();
    m_point_lights.Clear();

    command_list.CmdPopMarker();

    return true;
}

void Rndr::Shape3DRenderer::DrawCube(const Matrix4x4f& transform, Opal::Ref<const Material> material, f32 u_tiling, f32 v_tiling)
{
    RNDR_ASSERT(u_tiling > 0.0f, "U tiling multiplier must be positive!");
    RNDR_ASSERT(v_tiling > 0.0f, "V tiling multiplier must be positive!");
    Opal::StringUtf8 key(256, '\0');
    snprintf(key.GetData(), key.GetSize(), "Cube_%u_%u", static_cast<u32>(u_tiling), static_cast<u32>(v_tiling));
    key.Trim();

    if (!m_geometry_data.Contains(key))
    {
        ShapeGeometryData data;
        GenerateCube(m_vertex_data, m_index_data, data, u_tiling, v_tiling);
        m_geometry_data.Insert(key, data);
        m_is_geometry_data_dirty = true;
    }

    DrawShape(std::move(key), transform, material);
}

void Rndr::Shape3DRenderer::DrawSphere(const Matrix4x4f& transform, Opal::Ref<const Material> material, f32 u_tiling, f32 v_tiling,
                                       u32 latitude_segments, u32 longitude_segments)
{
    RNDR_ASSERT(latitude_segments > 0, "Latitude segments must be positive!");
    RNDR_ASSERT(longitude_segments > 0, "Longitude segments must be positive!");
    RNDR_ASSERT(u_tiling > 0.0f, "U tiling multiplier must be positive!");
    RNDR_ASSERT(v_tiling > 0.0f, "V tiling multiplier must be positive!");
    Opal::StringUtf8 key(256, '\0');
    snprintf(key.GetData(), key.GetSize(), "Sphere_%u_%u_%u_%u", latitude_segments, longitude_segments, static_cast<u32>(u_tiling),
             static_cast<u32>(v_tiling));
    key.Trim();

    if (!m_geometry_data.Contains(key))
    {
        ShapeGeometryData data;
        GenerateSphere(m_vertex_data, m_index_data, data, latitude_segments, longitude_segments, u_tiling, v_tiling);
        m_geometry_data.Insert(key, data);
        m_is_geometry_data_dirty = true;
    }

    DrawShape(std::move(key), transform, material);
}

void Rndr::Shape3DRenderer::DrawMesh(const Mesh& mesh, const Matrix4x4f& transform, Opal::Ref<const Material> material)
{
    if (!m_geometry_data.Contains(mesh.name))
    {
        ShapeGeometryData data;
        data.vertex_offset = static_cast<u32>(m_vertex_data.GetSize() / sizeof(VertexData));
        data.index_offset = static_cast<u32>(m_index_data.GetSize() / sizeof(u32));
        data.index_count = mesh.index_count;
        m_geometry_data.Insert(mesh.name, data);
        m_vertex_data.Append(mesh.vertices);
        m_index_data.Append(mesh.indices);
        m_is_geometry_data_dirty = true;
    }

    DrawShape(mesh.name, transform, material);
}

void Rndr::Shape3DRenderer::AddDirectionalLight(const Vector3f& direction, const Vector4f& color)
{
    m_directional_lights.PushBack({.direction = direction, .color = color});
}

void Rndr::Shape3DRenderer::AddPointLight(const Point3f& position, const Vector4f& color)
{
    m_point_lights.PushBack({.position = position, .color = color});
}

void Rndr::Shape3DRenderer::GenerateCube(Opal::DynamicArray<u8>& out_vertex_data, Opal::DynamicArray<u8>& out_index_data,
                                         ShapeGeometryData& out_data, f32 u_tiling, f32 v_tiling)
{
    const u32 vertex_offset = static_cast<u32>(out_vertex_data.GetSize() / sizeof(VertexData));
    const u32 index_offset = static_cast<u32>(out_index_data.GetSize() / sizeof(u32));
    float half = 0.5f;

    // Each face needs unique vertices for correct normals and UVs
    // 6 faces × 4 vertices = 24 vertices total

    // Face data: normal direction, then 4 corner positions
    // Format: nx, ny, nz, then for each corner: x, y, z, u, v
    struct FaceData
    {
        float normal[3];
        float verts[4][5];  // 4 vertices, each with x, y, z, u, v
    };

    FaceData faces[6] = {// Front face (Z+)
                         {{0.0f, 0.0f, 1.0f},
                          {{-half, -half, half, 0.0f, 0.0f},
                           {half, -half, half, 1.0f, 0.0f},
                           {half, half, half, 1.0f, 1.0f},
                           {-half, half, half, 0.0f, 1.0f}}},
                         // Back face (Z-)
                         {{0.0f, 0.0f, -1.0f},
                          {{half, -half, -half, 0.0f, 0.0f},
                           {-half, -half, -half, 1.0f, 0.0f},
                           {-half, half, -half, 1.0f, 1.0f},
                           {half, half, -half, 0.0f, 1.0f}}},
                         // Top face (Y+)
                         {{0.0f, 1.0f, 0.0f},
                          {{-half, half, half, 0.0f, 0.0f},
                           {half, half, half, 1.0f, 0.0f},
                           {half, half, -half, 1.0f, 1.0f},
                           {-half, half, -half, 0.0f, 1.0f}}},
                         // Bottom face (Y-)
                         {{0.0f, -1.0f, 0.0f},
                          {{-half, -half, -half, 0.0f, 0.0f},
                           {half, -half, -half, 1.0f, 0.0f},
                           {half, -half, half, 1.0f, 1.0f},
                           {-half, -half, half, 0.0f, 1.0f}}},
                         // Right face (X+)
                         {{1.0f, 0.0f, 0.0f},
                          {{half, -half, half, 0.0f, 0.0f},
                           {half, -half, -half, 1.0f, 0.0f},
                           {half, half, -half, 1.0f, 1.0f},
                           {half, half, half, 0.0f, 1.0f}}},
                         // Left face (X-)
                         {{-1.0f, 0.0f, 0.0f},
                          {{-half, -half, -half, 0.0f, 0.0f},
                           {-half, -half, half, 1.0f, 0.0f},
                           {-half, half, half, 1.0f, 1.0f},
                           {-half, half, -half, 0.0f, 1.0f}}}};

    // Generate vertices and indices for each face
    for (unsigned int f = 0; f < 6; ++f)
    {
        // Add 4 vertices for this face
        for (int v = 0; v < 4; ++v)
        {
            VertexData vertex;

            vertex.position[0] = faces[f].verts[v][0];
            vertex.position[1] = faces[f].verts[v][1];
            vertex.position[2] = faces[f].verts[v][2];

            vertex.normal[0] = faces[f].normal[0];
            vertex.normal[1] = faces[f].normal[1];
            vertex.normal[2] = faces[f].normal[2];

            vertex.tex_coord[0] = faces[f].verts[v][3] * u_tiling;
            vertex.tex_coord[1] = faces[f].verts[v][4] * v_tiling;

            out_vertex_data.Append(Opal::AsWritableBytes(vertex));
        }

        u32 idx0 = f * 4 + 0;
        u32 idx1 = f * 4 + 1;
        u32 idx2 = f * 4 + 2;
        u32 idx3 = f * 4 + 3;
        // Add 2 triangles (6 indices) for this face
        // Triangle 1: 0, 1, 2
        out_index_data.Append(Opal::AsWritableBytes(idx0));
        out_index_data.Append(Opal::AsWritableBytes(idx1));
        out_index_data.Append(Opal::AsWritableBytes(idx2));

        // Triangle 2: 0, 2, 3
        out_index_data.Append(Opal::AsWritableBytes(idx0));
        out_index_data.Append(Opal::AsWritableBytes(idx2));
        out_index_data.Append(Opal::AsWritableBytes(idx3));
    }

    out_data.vertex_offset = vertex_offset;
    out_data.index_offset = index_offset;
    out_data.index_count = static_cast<u32>(out_index_data.GetSize() / sizeof(u32)) - index_offset;
}

void Rndr::Shape3DRenderer::GenerateSphere(Opal::DynamicArray<u8>& out_vertex_data, Opal::DynamicArray<u8>& out_index_data,
                                           ShapeGeometryData& out_data, u32 latitude_segments, u32 longitude_segments, f32 u_tiling,
                                           f32 v_tiling)
{
    const u32 vertex_offset = static_cast<u32>(out_vertex_data.GetSize() / sizeof(VertexData));
    const u32 index_offset = static_cast<u32>(out_index_data.GetSize() / sizeof(u32));

    for (u32 lat = 0; lat <= latitude_segments; ++lat)
    {
        const f32 theta = lat * Opal::k_pi_float / latitude_segments;  // 0 to PI (top to bottom)
        const f32 sin_theta = Opal::Sin(theta);
        const f32 cos_theta = Opal::Cos(theta);

        for (u32 lon = 0; lon <= longitude_segments; ++lon)
        {
            const f32 phi = lon * 2.0f * Opal::k_pi_float / longitude_segments;  // 0 to 2PI (around)
            const f32 sin_phi = Opal::Sin(phi);
            const f32 cos_phi = Opal::Cos(phi);

            // Calculate vertex position on unit sphere, then scale by radius
            const f32 x = cos_phi * sin_theta;
            const f32 y = cos_theta;
            const f32 z = sin_phi * sin_theta;

            // UV coordinates
            const f32 u = (static_cast<float>(lon) / longitude_segments) * u_tiling;
            const f32 v = (static_cast<float>(lat) / latitude_segments) * v_tiling;

            VertexData vertex;
            vertex.position.x = x;
            vertex.position.y = y;
            vertex.position.z = z;
            vertex.normal.x = x;
            vertex.normal.y = y;
            vertex.normal.z = z;
            vertex.tex_coord.x = u;
            vertex.tex_coord.y = v;

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

    out_data.vertex_offset = vertex_offset;
    out_data.index_offset = index_offset;
    out_data.index_count = static_cast<u32>(out_index_data.GetSize() / sizeof(u32)) - index_offset;
}

void Rndr::Shape3DRenderer::DrawShape(Opal::StringUtf8 key, const Matrix4x4f& transform, Opal::Ref<const Material> material)
{
    auto geometry_data_it = m_geometry_data.Find(key);
    RNDR_ASSERT(geometry_data_it != m_geometry_data.end(), "Failed to find material in map!");
    const ShapeGeometryData& geometry_data = geometry_data_it.GetValue();

    const MaterialKey material_key{material};
    auto it = m_materials.Find(material_key);
    if (it == m_materials.end())
    {

        const PerMaterialData material_data;
        m_materials.Insert(material_key, material_data);
        it = m_materials.Find(material_key);
        RNDR_ASSERT(it != m_materials.end(), "Failed to find material in map!");
    }
    PerMaterialData& material_data = it.GetValue();

    auto pipeline_it = m_pipelines.Find(material_key);
    if (pipeline_it == m_pipelines.end())
    {
        ShaderPermutation shader_permutation = CreateShaderPermutationFromMaterial(material);
        Pipeline pipeline = CreatePipeline(shader_permutation);
        m_shader_permutations.PushBack(std::move(shader_permutation));
        m_pipelines.Insert(material_key, std::move(pipeline));
    }

    InstanceData instance_data;
    instance_data.model_transform = Opal::Transpose(transform);
    instance_data.normal_transform = Opal::Inverse(transform);
    instance_data.albedo_color = material->GetAlbedoColor();
    instance_data.emissive_color = material->GetEmissiveColor();
    instance_data.roughness = material->GetRoughness();
    instance_data.metallic_factor = material->GetMetalicFactor();
    instance_data.transparency_factor = material->GetTransparencyFactor();
    instance_data.alpha_test = material->GetAlphaTest();
    instance_data.flags = static_cast<u32>(material->GetMaterialFlags());
    material_data.instances.PushBack(instance_data);

    DrawIndicesData draw_data;
    draw_data.index_count = geometry_data.index_count;
    draw_data.base_vertex = static_cast<u32>(geometry_data.vertex_offset);
    draw_data.first_index = static_cast<u32>(geometry_data.index_offset);
    draw_data.instance_count = 1;
    material_data.draw_commands.PushBack(draw_data);
}

Rndr::ShaderPermutation Rndr::Shape3DRenderer::CreateShaderPermutationFromMaterial(const Material& material)
{
    ShaderDesc shader_desc;
    shader_desc.type = ShaderType::Fragment;
    shader_desc.source = m_fragment_shader_source;
    if (material.HasAlbedoTexture())
    {
        shader_desc.defines.PushBack("USE_ALBEDO_TEXTURE=1");
    }
    if (material.HasEmissiveTexture())
    {
        shader_desc.defines.PushBack("USE_EMISSIVE_TEXTURE=1");
    }
    if (material.HasMetalicRoughnessTexture())
    {
        shader_desc.defines.PushBack("USE_METALLIC_ROUGHNESS_TEXTURE=1");
    }
    if (material.HasNormalTexture())
    {
        shader_desc.defines.PushBack("USE_NORMAL_TEXTURE=1");
    }
    if (material.HasAmbientOcclusionTexture())
    {
        shader_desc.defines.PushBack("USE_AMBIENT_OCCLUSION_TEXTURE=1");
    }
    if (material.HasOpacityTexture())
    {
        shader_desc.defines.PushBack("USE_OPACITY_TEXTURE=1");
    }

    ShaderPermutation shader_permutation(m_desc.graphics_context, shader_desc);
    if (!shader_permutation.IsValid())
    {
        throw Opal::Exception("Failed to create shader permutation");
    }
    return shader_permutation;
}

Rndr::Pipeline Rndr::Shape3DRenderer::CreatePipeline(ShaderPermutation& shader_permutation)
{
    const InputLayoutDesc input_layout_desc = Rndr::InputLayoutBuilder()
                                                  .AddShaderStorage(m_vertex_buffer, 1)
                                                  .AddShaderStorage(m_model_transform_buffer, 2)
                                                  .AddIndexBuffer(m_index_buffer)
                                                  .Build();
    return Pipeline(m_desc.graphics_context, PipelineDesc{.vertex_shader = &m_vertex_shader,
                                                          .pixel_shader = shader_permutation.GetShader().GetPtr(),
                                                          .input_layout = input_layout_desc,
                                                          .depth_stencil = {.is_depth_enabled = true}});
}

namespace Opal
{
template <>
struct Hasher<Rndr::Material>
{
    u64 operator()(const Rndr::Material& material) const
    {
        u64 hash = material.m_bit_mask;
        if ((material.m_bit_mask & 1) != 0)
        {
            hash ^= Hasher<StringUtf8>()(material.m_desc.albedo_texture_path);
        }
        if ((material.m_bit_mask & 2) != 0)
        {
            hash ^= Hasher<StringUtf8>()(material.m_desc.emissive_texture_path);
        }
        if ((material.m_bit_mask & 4) != 0)
        {
            hash ^= Hasher<StringUtf8>()(material.m_desc.metallic_roughness_texture_path);
        }
        if ((material.m_bit_mask & 8) != 0)
        {
            hash ^= Hasher<StringUtf8>()(material.m_desc.normal_texture_path);
        }
        if ((material.m_bit_mask & 16) != 0)
        {
            hash ^= Hasher<StringUtf8>()(material.m_desc.ambient_occlusion_texture_path);
        }
        if ((material.m_bit_mask & 32) != 0)
        {
            hash ^= Hasher<StringUtf8>()(material.m_desc.opacity_texture_path);
        }
        return hash;
    }
};

template <>
struct Hasher<Rndr::Shape3DRenderer::MaterialKey>
{
    u64 operator()(const Rndr::Shape3DRenderer::MaterialKey& material_key) const
    {
        const Rndr::Material& material = *material_key.material;
        return Hasher<Rndr::Material>()(material);
    }
};
}  // namespace Opal
