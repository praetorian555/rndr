#pragma once

#include "opal/container/hash-map.h"

#include "rndr/material.hpp"
#include "rndr/renderers/renderer-base.hpp"

namespace Rndr
{

class Shape3DRenderer : public RendererBase
{
public:
    struct MaterialKey
    {
        Opal::Ref<const Material> material;

        bool operator==(const MaterialKey& other) const { return *material == *other.material; }
    };

    Shape3DRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc, Opal::Ref<FrameBuffer> target);
    ~Shape3DRenderer() override;

    void Destroy();

    void SetFrameBufferTarget(Opal::Ref<FrameBuffer> target) { m_target = target; }
    void SetTransforms(const Matrix4x4f& view, const Matrix4x4f& projection);

    bool Render(f32 delta_seconds, CommandList& command_list) override;

    void DrawCube(const Matrix4x4f& transform, Opal::Ref<const Material> material, f32 u_tiling = 1.0f, f32 v_tiling = 1.0f);
    void DrawSphere(const Matrix4x4f& transform, Opal::Ref<const Material> material, f32 u_tiling = 1.0f, f32 v_tiling = 1.0f,
                    u32 latitude_segments = 128, u32 longitude_segments = 128);
    // void DrawCylinder(const Matrix4x4f& transform, const Vector4f& color);
    // void DrawCone(const Matrix4x4f& transform, const Vector4f& color);

private:
    constexpr static u32 k_max_vertex_count = 1'000'000;
    constexpr static u32 k_max_index_count = 1'000'000;

    struct VertexData
    {
        Point3f position;
        Vector3f normal;
        Vector2f tex_coord;
    };

    struct InstanceData
    {
        Matrix4x4f model_transform;
        Matrix4x4f normal_transform;
        Vector4f color;
    };

    struct PerMaterialData
    {
        Opal::DynamicArray<InstanceData> instances;
        Opal::DynamicArray<DrawIndicesData> draw_commands;
    };

    enum class ShapeType : u8
    {
        Cube = 0,
        Sphere,
        Cylinder,
        Cone,
        Count
    };

    struct ShapeGeometryData
    {
        u64 vertex_offset = 0;
        u64 index_offset = 0;
        u32 index_count = 0;
    };

    static void GenerateCube(Opal::DynamicArray<VertexData>& out_vertex_data, Opal::DynamicArray<u32>& out_index_data,
                             ShapeGeometryData& out_data, f32 u_tiling = 1.0f, f32 v_tiling = 1.0f);
    static void GenerateSphere(Opal::DynamicArray<VertexData>& out_vertex_data, Opal::DynamicArray<u32>& out_index_data,
                               ShapeGeometryData& out_data, u32 latitude_segments = 32, u32 longitude_segments = 32, f32 u_tiling = 1.0f,
                               f32 v_tiling = 1.0f);
    void DrawShape(Opal::StringUtf8 key, const Matrix4x4f& transform, Opal::Ref<const Material> material);

    Opal::Ref<FrameBuffer> m_target;
    Matrix4x4f m_view;
    Matrix4x4f m_projection;

    Shader m_vertex_shader;
    Shader m_fragment_color_shader;
    Shader m_fragment_texture_shader;
    Buffer m_per_frame_buffer;
    Buffer m_model_transform_buffer;
    Buffer m_vertex_buffer;
    Buffer m_index_buffer;
    Buffer m_draw_commands_buffer;
    Pipeline m_color_pipeline;
    Pipeline m_texture_pipeline;

    Opal::HashMap<MaterialKey, PerMaterialData> m_materials;
    Opal::DynamicArray<VertexData> m_vertex_data;
    Opal::DynamicArray<u32> m_index_data;
    bool m_is_geometry_data_dirty = false;
    Opal::HashMap<Opal::StringUtf8, ShapeGeometryData> m_geometry_data;
};

}  // namespace Rndr
