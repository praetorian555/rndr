#include "rndr/renderers/shape-3d-renderer.hpp"

#include "opal/paths.h"

#include "rndr/file.hpp"
#include "rndr/input-layout-builder.hpp"

namespace
{
OPAL_START_DISABLE_WARNINGS
OPAL_DISABLE_MSVC_WARNING(4324)
struct PerFrameData
{
    Rndr::Matrix4x4f view_projection_transform;
    alignas(16) Rndr::Point3f camera_position_world;
    alignas(16) Rndr::Vector3f light_direction_world = {1.0f, 1.0f, 1.0f};
};
OPAL_END_DISABLE_WARNINGS
}  // namespace

Rndr::Shape3DRenderer::Shape3DRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc, Opal::Ref<FrameBuffer> target)
    : RendererBase(name, desc), m_target(target)
{
    const Opal::StringUtf8 shader_dir = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "shaders");
    const Opal::StringUtf8 vertex_shader_contents = File::ReadShader(shader_dir, "pbr.vert");
    const Opal::StringUtf8 fragment_shader_contents = File::ReadShader(shader_dir, "pbr.frag");

    m_vertex_shader =
        Shader(m_desc.graphics_context,
               {.type = ShaderType::Vertex, .source = vertex_shader_contents, .debug_name = "Shape 3D Renderer - Vertex Shader"});
    RNDR_ASSERT(m_vertex_shader.IsValid(), "Failed to create vertex shader!");
    m_fragment_color_shader = Shader(
        m_desc.graphics_context,
        {.type = ShaderType::Fragment, .source = fragment_shader_contents, .debug_name = "Shape 3D Renderer - Fragment Color Shader"});
    RNDR_ASSERT(m_fragment_color_shader.IsValid(), "Failed to create fragment shader!");
    m_fragment_texture_shader = Shader(m_desc.graphics_context, {.type = ShaderType::Fragment,
                                                                 .source = fragment_shader_contents,
                                                                 .defines = {"USE_ALBEDO_TEXTURE=1"},
                                                                 .debug_name = "Shape 3D Renderer - Fragment Texture Shader"});
    RNDR_ASSERT(m_fragment_texture_shader.IsValid(), "Failed to create fragment shader!");

    Opal::DynamicArray<VertexData> vertex_data;
    Opal::DynamicArray<u32> index_data;
    SetupGeometryData(vertex_data, index_data);

    m_vertex_buffer = Buffer(desc.graphics_context,
                             {.type = BufferType::ShaderStorage,
                              .usage = Usage::Default,
                              .size = vertex_data.GetSize() * sizeof(VertexData),
                              .stride = sizeof(VertexData),
                              .debug_name = "Shape 3D Renderer - Vertex Buffer"},
                             Opal::AsBytes(vertex_data));
    RNDR_ASSERT(m_vertex_buffer.IsValid(), "Failed to create vertex buffer!");
    m_index_buffer = Buffer(desc.graphics_context,
                            {.type = BufferType::Index,
                             .usage = Usage::Default,
                             .size = index_data.GetSize() * sizeof(uint32_t),
                             .stride = sizeof(uint32_t),
                             .debug_name = "Shape 3D Renderer - Index Buffer"},
                            Opal::AsBytes(index_data));
    RNDR_ASSERT(m_index_buffer.IsValid(), "Failed to create index buffer!");
    m_model_transform_buffer = Buffer(desc.graphics_context, {.type = BufferType::ShaderStorage,
                                                              .usage = Usage::Dynamic,
                                                              .size = 10000 * sizeof(InstanceData),
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
                                                              .size = 10000 * sizeof(DrawIndicesData),
                                                              .stride = sizeof(DrawIndicesData),
                                                              .debug_name = "Shape 3D Renderer - Draw Commands Buffer"});
    RNDR_ASSERT(m_draw_commands_buffer.IsValid(), "Failed to create draw commands buffer!");

    const InputLayoutDesc input_layout_desc = Rndr::InputLayoutBuilder()
                                                  .AddShaderStorage(m_vertex_buffer, 1)
                                                  .AddShaderStorage(m_model_transform_buffer, 2)
                                                  .AddIndexBuffer(m_index_buffer)
                                                  .Build();
    m_color_pipeline =
        Pipeline(desc.graphics_context, {.vertex_shader = &m_vertex_shader,
                                         .pixel_shader = &m_fragment_color_shader,
                                         .input_layout = input_layout_desc,
                                         .rasterizer = {.fill_mode = FillMode::Solid, .front_face_winding_order = WindingOrder::CCW},
                                         .depth_stencil = {.is_depth_enabled = true},
                                         .debug_name = "Shape 3D Renderer - Solid Color Pipeline"});
    RNDR_ASSERT(m_color_pipeline.IsValid(), "Failed to create pipeline!");

    m_texture_pipeline =
        Pipeline(desc.graphics_context, {.vertex_shader = &m_vertex_shader,
                                         .pixel_shader = &m_fragment_texture_shader,
                                         .input_layout = input_layout_desc,
                                         .rasterizer = {.fill_mode = FillMode::Solid, .front_face_winding_order = WindingOrder::CCW},
                                         .depth_stencil = {.is_depth_enabled = true},
                                         .debug_name = "Shape 3D Renderer - Solid Texture Pipeline"});
    RNDR_ASSERT(m_texture_pipeline.IsValid(), "Failed to create pipeline!");
}

Rndr::Shape3DRenderer::~Shape3DRenderer()
{
    Destroy();
}

void Rndr::Shape3DRenderer::Destroy()
{
    m_color_pipeline.Destroy();
    m_texture_pipeline.Destroy();
    m_per_frame_buffer.Destroy();
    m_model_transform_buffer.Destroy();
    m_index_buffer.Destroy();
    m_vertex_buffer.Destroy();
    m_draw_commands_buffer.Destroy();
    m_fragment_color_shader.Destroy();
    m_fragment_texture_shader.Destroy();
    m_vertex_shader.Destroy();
}

void Rndr::Shape3DRenderer::SetTransforms(const Matrix4x4f& view, const Matrix4x4f& projection)
{
    m_view = view;
    m_projection = projection;
}

bool Rndr::Shape3DRenderer::Render(f32 delta_seconds, CommandList& command_list)
{
    RNDR_UNUSED(delta_seconds);

    PerFrameData per_frame_data;
    per_frame_data.view_projection_transform = Opal::Transpose(m_projection * m_view);
    per_frame_data.camera_position_world = Point3f::Zero();  // TODO: Get from the controller
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
        const Material* material = pair.key.material.GetPtr();
        PerMaterialData& material_data = pair.value;
        const Pipeline* pipeline = nullptr;
        command_list.CmdBindBuffer(m_per_frame_buffer, 0);
        command_list.CmdUpdateBuffer(m_model_transform_buffer, Opal::AsBytes(material_data.instances));
        if (material->HasAlbedoTexture())
        {
            pipeline = &m_texture_pipeline;
            command_list.CmdBindTexture(material->GetAlbedoTexture(), 0);
        }
        else
        {
            pipeline = &m_color_pipeline;
        }
        command_list.CmdBindPipeline(*pipeline);
        command_list.CmdDrawIndicesMulti(Opal::Ref{m_draw_commands_buffer}, PrimitiveTopology::Triangle,
                                         Opal::ArrayView<DrawIndicesData>(material_data.draw_commands));
    }

    m_materials.Clear();

    return true;
}

void Rndr::Shape3DRenderer::DrawCube(const Matrix4x4f& transform, Opal::Ref<const Material> material)
{
    DrawShape(ShapeType::Cube, transform, material);
}

void Rndr::Shape3DRenderer::DrawSphere(const Matrix4x4f& transform, Opal::Ref<const Material> material)
{
    DrawShape(ShapeType::Sphere, transform, material);
}

void Rndr::Shape3DRenderer::SetupGeometryData(Opal::DynamicArray<VertexData>& out_vertex_data, Opal::DynamicArray<u32>& out_index_data)
{
    m_geometry_data.Resize(static_cast<u64>(ShapeType::Count));

    // Generate cube data
    {
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

                vertex.tex_coord[0] = faces[f].verts[v][3];
                vertex.tex_coord[1] = faces[f].verts[v][4];

                out_vertex_data.PushBack(vertex);
            }

            // Add 2 triangles (6 indices) for this face
            // Triangle 1: 0, 1, 2
            out_index_data.PushBack(f * 4 + 0);
            out_index_data.PushBack(f * 4 + 1);
            out_index_data.PushBack(f * 4 + 2);

            // Triangle 2: 0, 2, 3
            out_index_data.PushBack(f * 4 + 0);
            out_index_data.PushBack(f * 4 + 2);
            out_index_data.PushBack(f * 4 + 3);
        }

        ShapeGeometryData& data = m_geometry_data[static_cast<u32>(ShapeType::Cube)];
        data.vertex_offset = 0;
        data.index_offset = 0;
        data.index_count = static_cast<u32>(out_index_data.GetSize());
    }

    // Generate sphere data
    {
        const u32 vertex_offset = static_cast<u32>(out_vertex_data.GetSize());
        const u32 index_offset = static_cast<u32>(out_index_data.GetSize());
        constexpr u32 k_latitude_segments = 32;
        constexpr u32 k_longitude_segments = 32;

        for (u32 lat = 0; lat <= k_latitude_segments; ++lat)
        {
            const f32 theta = lat * Opal::k_pi_float / k_latitude_segments;  // 0 to PI (top to bottom)
            const f32 sin_theta = Opal::Sin(theta);
            const f32 cos_theta = Opal::Cos(theta);

            for (u32 lon = 0; lon <= k_longitude_segments; ++lon)
            {
                const f32 phi = lon * 2.0f * Opal::k_pi_float / k_longitude_segments;  // 0 to 2PI (around)
                const f32 sin_phi = Opal::Sin(phi);
                const f32 cos_phi = Opal::Cos(phi);

                // Calculate vertex position on unit sphere, then scale by radius
                const f32 x = cos_phi * sin_theta;
                const f32 y = cos_theta;
                const f32 z = sin_phi * sin_theta;

                // UV coordinates
                const f32 u = static_cast<float>(lon) / k_longitude_segments;
                const f32 v = static_cast<float>(lat) / k_latitude_segments;

                VertexData vertex;
                vertex.position.x = x;
                vertex.position.y = y;
                vertex.position.z = z;
                vertex.normal.x = x;
                vertex.normal.y = y;
                vertex.normal.z = z;
                vertex.tex_coord.x = u;
                vertex.tex_coord.y = v;

                out_vertex_data.PushBack(vertex);
            }
        }

        for (u32 lat = 0; lat < k_latitude_segments; ++lat)
        {
            for (u32 lon = 0; lon < k_longitude_segments; ++lon)
            {
                const u32 current = lat * (k_longitude_segments + 1) + lon;
                const u32 next = current + k_longitude_segments + 1;
                out_index_data.PushBack(current);
                out_index_data.PushBack(current + 1);
                out_index_data.PushBack(next);
                out_index_data.PushBack(current + 1);
                out_index_data.PushBack(next + 1);
                out_index_data.PushBack(next);
            }
        }

        ShapeGeometryData& data = m_geometry_data[static_cast<u32>(ShapeType::Sphere)];
        data.vertex_offset = vertex_offset;
        data.index_offset = index_offset;
        data.index_count = static_cast<u32>(out_index_data.GetSize()) - index_offset;
    }
}

void Rndr::Shape3DRenderer::DrawShape(ShapeType shape_type, const Matrix4x4f& transform, Opal::Ref<const Material> material)
{
    MaterialKey material_key{material};
    auto it = m_materials.Find(material_key);
    if (it == m_materials.end())
    {
        PerMaterialData material_data;
        m_materials.Insert(material_key, material_data);
        it = m_materials.Find(material_key);
        RNDR_ASSERT(it != m_materials.end(), "Failed to find material in map!");
    }
    PerMaterialData& material_data = it.GetValue();

    InstanceData instance_data;
    instance_data.model_transform = Opal::Transpose(transform);
    instance_data.normal_transform = Opal::Inverse(transform);
    instance_data.color = material->GetAlbedoColor();
    material_data.instances.PushBack(instance_data);

    const ShapeGeometryData& geometry_data = m_geometry_data[static_cast<u32>(shape_type)];

    DrawIndicesData draw_data;
    draw_data.index_count = geometry_data.index_count;
    draw_data.base_vertex = static_cast<u32>(geometry_data.vertex_offset);
    draw_data.first_index = static_cast<u32>(geometry_data.index_offset);
    draw_data.instance_count = 1;
    material_data.draw_commands.PushBack(draw_data);
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
