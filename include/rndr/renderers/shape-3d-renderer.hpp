#pragma once

#include "opal/paths.h"

#include "rndr/renderers/renderer-base.hpp"

namespace Rndr
{

class Shape3DRenderer : public RendererBase
{
public:
    Shape3DRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc, Opal::Ref<FrameBuffer> target);
    ~Shape3DRenderer() override;

    void Destroy();

    void SetFrameBufferTarget(Opal::Ref<FrameBuffer> target) { m_target = target; }
    void SetTransforms(const Matrix4x4f& view, const Matrix4x4f& projection);

    bool Render(f32 delta_seconds, CommandList& command_list) override;

    void DrawCube(const Matrix4x4f& transform, const Vector4f& color);
    void DrawSphere(const Matrix4x4f& transform, const Vector4f& color);
    void DrawCylinder(const Matrix4x4f& transform, const Vector4f& color);
    void DrawCone(const Matrix4x4f& transform, const Vector4f& color);

private:
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

    void SetupGeometryData(Opal::DynamicArray<VertexData>& out_vertex_data, Opal::DynamicArray<u32>& out_index_data);
    void DrawShape(ShapeType shape_type, const Matrix4x4f& transform, const Vector4f& color);

    Opal::Ref<FrameBuffer> m_target;
    Matrix4x4f m_view;
    Matrix4x4f m_projection;

    Shader m_vertex_shader;
    Shader m_fragment_shader;
    Buffer m_per_frame_buffer;
    Buffer m_model_transform_buffer;
    Buffer m_vertex_buffer;
    Buffer m_index_buffer;
    Pipeline m_pipeline;

    Opal::DynamicArray<ShapeGeometryData> m_geometry_data;
    Opal::DynamicArray<InstanceData> m_instances;
    Opal::DynamicArray<DrawIndicesData> m_draw_commands;
};

}  // namespace Rndr
