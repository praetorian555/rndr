#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/hash-map.h"
#include "opal/container/in-place-array.h"
#include "opal/container/ref.h"
#include "opal/container/string.h"

#include "rndr/canvas/brush.hpp"
#include "rndr/canvas/buffer.hpp"
#include "rndr/canvas/draw-list.hpp"
#include "rndr/canvas/mesh.hpp"
#include "rndr/canvas/shader.hpp"
#include "rndr/canvas/texture.hpp"
#include "rndr/colors.hpp"
#include "rndr/math.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

struct Mesh;

namespace Canvas
{

class Context;

/**
 * Material description for PBR rendering. Texture pointers are optional; when null the
 * corresponding scalar value is used instead. The renderer computes the material_flags
 * bitmask automatically from which textures are set.
 */
struct PbrMaterialDesc
{
    Opal::StringUtf8 material_name;

    Vector4f albedo_color = Colors::k_pink;
    Vector4f emissive_color = {0, 0, 0, 0};
    Vector4f roughness = {0.6f, 0.6f, 0.0f, 0.0f};
    f32 metallic_factor = 0.2f;
    f32 transparency_factor = 1.0f;
    f32 alpha_test = 0.0f;

    Opal::Ref<const Texture> albedo_texture;
    Opal::Ref<const Texture> emissive_texture;
    Opal::Ref<const Texture> metallic_roughness_texture;
    Opal::Ref<const Texture> normal_texture;
    Opal::Ref<const Texture> ambient_occlusion_texture;
    Opal::Ref<const Texture> opacity_texture;
};

struct DirectionalLight
{
    Vector3f direction;
    Vector4f color;
};

struct PointLight
{
    Point3f position;
    Vector4f color;
};

/**
 * Renders 3D meshes with PBR (physically-based rendering) materials. Uses a single shader
 * with a material_flags uniform to dynamically select which textures to sample, avoiding
 * shader permutations.
 *
 * Instances sharing the same geometry and texture set are batched into a single instanced
 * draw call via an SSBO.
 *
 * Usage:
 * @code
 *   PbrRenderer renderer(context);
 *   // Each frame:
 *   renderer.BeginFrame();
 *   renderer.SetViewProjection(vp);
 *   renderer.SetCameraPosition(cam_pos);
 *   renderer.AddDirectionalLight({1,1,1}, Colors::k_white);
 *   renderer.DrawCube(transform, material);
 *   renderer.DrawSphere(transform, material);
 *   renderer.DrawMesh("helmet", mesh_data, transform, material);
 *   renderer.Render(draw_list);
 * @endcode
 */
class PbrRenderer
{
public:
    explicit PbrRenderer(Opal::Ref<Context> context);
    ~PbrRenderer();

    PbrRenderer(const PbrRenderer&) = delete;
    PbrRenderer& operator=(const PbrRenderer&) = delete;
    PbrRenderer(PbrRenderer&&) noexcept = default;
    PbrRenderer& operator=(PbrRenderer&&) noexcept = default;

    void Destroy();

    /** Reset per-frame state (draw entries, lights). Call at the start of each frame. */
    void BeginFrame();

    /** Set the combined view-projection matrix for this frame. */
    void SetViewProjection(const Matrix4x4f& view_projection);

    /** Set the camera position in world space (needed for specular lighting). */
    void SetCameraPosition(const Point3f& camera_position);

    void AddDirectionalLight(const Vector3f& direction, const Vector4f& color);
    void AddPointLight(const Point3f& position, const Vector4f& color);

    /**
     * Draw a unit cube centered at the origin, transformed by @p transform.
     * Geometry is generated once and cached.
     */
    void DrawCube(const Matrix4x4f& transform, const PbrMaterialDesc& material, f32 u_tiling = 1.0f, f32 v_tiling = 1.0f);

    /**
     * Draw a unit sphere centered at the origin, transformed by @p transform.
     * Geometry is generated once and cached.
     */
    void DrawSphere(const Matrix4x4f& transform, const PbrMaterialDesc& material, f32 u_tiling = 1.0f, f32 v_tiling = 1.0f,
                    u32 latitude_segments = 32, u32 longitude_segments = 32);

    /**
     * Draw an arbitrary mesh. The mesh data is uploaded once and cached by @p key.
     * @param key Unique string identifying this geometry (used for caching).
     * @param mesh_data CPU-side mesh data (vertices must match the PBR vertex layout: position3, normal3, texcoord2).
     * @param transform Model transform.
     * @param material PBR material description.
     */
    void DrawMesh(const Opal::StringUtf8& key, const Rndr::Mesh& mesh_data, const Matrix4x4f& transform, const PbrMaterialDesc& material);

    /** Record all draw commands into the draw list. */
    void Render(DrawList& draw_list);

private:
    static constexpr u32 k_max_instance_count = 100'000;
    static constexpr u32 k_max_light_count = 4;

    static constexpr u32 k_flag_albedo_texture = 1 << 0;
    static constexpr u32 k_flag_emissive_texture = 1 << 1;
    static constexpr u32 k_flag_metallic_roughness_texture = 1 << 2;
    static constexpr u32 k_flag_normal_texture = 1 << 3;
    static constexpr u32 k_flag_ambient_occlusion_texture = 1 << 4;
    static constexpr u32 k_flag_opacity_texture = 1 << 5;

    struct InstanceData
    {
        Matrix4x4f model_transform;
        Matrix4x4f normal_transform;
        Vector4f albedo_color;
        Vector4f emissive_color;
        Vector4f roughness;
        f32 metallic_factor;
        f32 transparency_factor;
        f32 alpha_test;
        u32 material_flags;
    };

    /** Key for grouping draw calls: same geometry + same texture set. */
    struct BatchKey : Opal::ClonableBase<BatchKey>
    {
        Opal::StringUtf8 geometry_key;
        Opal::InPlaceArray<Opal::Ref<const Texture>, 6> textures;
        OPAL_CLONE_FIELDS(geometry_key, textures);

        bool operator==(const BatchKey& other) const;

        friend struct Opal::Hasher<BatchKey>;
    };

    friend struct Opal::Hasher<BatchKey>;

    struct BatchData
    {
        Opal::DynamicArray<InstanceData> instances;
        Brush brush;
        Buffer instance_buffer;
    };

    static u32 ComputeMaterialFlags(const PbrMaterialDesc& material);
    InstanceData MakeInstanceData(const Matrix4x4f& transform, const PbrMaterialDesc& material);
    void EnsureGeometry(const Opal::StringUtf8& key, const Opal::ArrayView<const u8>& vertex_data,
                        const Opal::ArrayView<const u8>& index_data);
    void AddDrawEntry(const Opal::StringUtf8& geometry_key, const Matrix4x4f& transform, const PbrMaterialDesc& material);
    void BindTextures(Brush& brush, const BatchKey& key);

    static void GenerateCube(Opal::DynamicArray<u8>& out_vertex_data, Opal::DynamicArray<u8>& out_index_data, f32 u_tiling, f32 v_tiling);
    static void GenerateSphere(Opal::DynamicArray<u8>& out_vertex_data, Opal::DynamicArray<u8>& out_index_data, u32 latitude_segments,
                               u32 longitude_segments, f32 u_tiling, f32 v_tiling);

    Opal::Ref<Context> m_context;
    Shader m_shader;
    Texture m_dummy_texture;

    Matrix4x4f m_view_projection;
    Point3f m_camera_position;
    Opal::DynamicArray<DirectionalLight> m_directional_lights;
    Opal::DynamicArray<PointLight> m_point_lights;

    Opal::HashMap<Opal::StringUtf8, Mesh> m_geometry_cache;
    Opal::HashMap<BatchKey, BatchData> m_batches;
};

}  // namespace Canvas
}  // namespace Rndr

namespace Opal
{
template <>
struct Hasher<Rndr::Canvas::PbrRenderer::BatchKey>
{
    u64 operator()(const Rndr::Canvas::PbrRenderer::BatchKey& key) const;
};
}  // namespace Opal
